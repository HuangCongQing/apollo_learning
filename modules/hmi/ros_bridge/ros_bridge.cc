/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include <chrono>
#include <memory>
#include <thread>

#include "gflags/gflags.h"
#include "modules/canbus/proto/chassis.pb.h"
#include "modules/common/adapters/adapter_manager.h"
#include "modules/common/log.h"
#include "modules/common/util/file.h"
#include "modules/control/common/control_gflags.h"
#include "modules/control/proto/pad_msg.pb.h"
#include "modules/map/hdmap/hdmap_util.h"


DEFINE_string(adapter_config_file,
              "modules/hmi/conf/ros_bridge_adapter.pb.txt",
              "Adapter config file for ros bridge.");

DEFINE_string(routing_request_template,
              "modules/hmi/conf/routing_request_template.pb.txt",
              "RoutingRequest template file.");

using apollo::common::adapter::AdapterManager;
using apollo::common::adapter::AdapterManagerConfig;
using apollo::control::DrivingAction;
using apollo::canbus::Chassis;
using apollo::hdmap::HDMapUtil;

namespace apollo {
namespace hmi {
namespace {

static constexpr char kHMIRosBridgeName[] = "hmi_ros_bridge";

class RosBridge {
 public:
  void Init() {
    // Init AdapterManager.
    AdapterManagerConfig adapter_conf;
    CHECK(apollo::common::util::GetProtoFromASCIIFile(FLAGS_adapter_config_file,
                                                      &adapter_conf));
    AdapterManager::Init(adapter_conf);
    AdapterManager::AddHMICommandCallback(OnHMICommand);

    // Init RoutingRequest template.
    CHECK(apollo::common::util::GetProtoFromASCIIFile(
        FLAGS_routing_request_template, &routing_request_template));
    // Init HDMap.
    CHECK(HDMapUtil::instance()->BaseMap());
  }

 private:
  static void OnHMICommand(const HMICommand& command) {
    if (command.has_change_driving_mode()) {
      const auto& cmd = command.change_driving_mode();
      if (cmd.reset_first()) {
        ChangeDrivingModeTo(Chassis::COMPLETE_MANUAL);
      }
      ChangeDrivingModeTo(cmd.target_mode());
    }

    if (command.new_routing_request()) {
      instance()->SendRoutingRequest();
    }
  }

  static bool ChangeDrivingModeTo(const Chassis::DrivingMode target_mode) {
    AINFO << "RosBridge is changing driving mode to " << target_mode;
    auto driving_action = DrivingAction::RESET;
    switch (target_mode) {
      case Chassis::COMPLETE_MANUAL:
        // Default driving action: RESET.
        break;
      case Chassis::COMPLETE_AUTO_DRIVE:
        driving_action = DrivingAction::START;
        break;
      default:
        AFATAL << "Unknown action to change driving mode to " << target_mode;
    }

    constexpr int kMaxTries = 3;
    constexpr auto kTryInterval = std::chrono::milliseconds(500);
    auto* chassis = AdapterManager::GetChassis();
    for (int i = 0; i < kMaxTries; ++i) {
      // Send driving action periodically until entering target driving mode.
      SendPadMessage(driving_action);
      std::this_thread::sleep_for(kTryInterval);

      chassis->Observe();
      if (chassis->Empty()) {
        AERROR << "No Chassis message received!";
      } else if (chassis->GetLatestObserved().driving_mode() == target_mode) {
        return true;
      }
    }
    AERROR << "Failed to change driving mode to " << target_mode;
    return false;
  }

  static void SendPadMessage(const DrivingAction action) {
    control::PadMessage pad;
    pad.set_action(action);
    AdapterManager::FillPadHeader(kHMIRosBridgeName, &pad);
    AdapterManager::PublishPad(pad);
    AINFO << "Sent PadMessage";
  }

  void SendRoutingRequest() {
    // Observe position from Localization.
    auto* localization = AdapterManager::GetLocalization();
    localization->Observe();
    if (localization->Empty()) {
      AERROR << "No Localization message received!";
      return;
    }
    const auto& pos = localization->GetLatestObserved().pose().position();

    // Look up lane info from map.
    apollo::hdmap::LaneInfoConstPtr lane = nullptr;
    double s, l;

    HDMapUtil::instance()->BaseMapRef().get_nearest_lane(pos, &lane, &s, &l);
    if (lane == nullptr) {
      AERROR << "Cannot get nearest lane from current position.";
      return;
    }

    // Populate message and send.
    routing::RoutingRequest routing_request = routing_request_template;
    auto* start_point = routing_request.mutable_start();
    start_point->set_id(lane->id().id());
    start_point->set_s(s);
    auto* pose = start_point->mutable_pose();
    pose->set_x(pos.x());
    pose->set_y(pos.y());
    pose->set_z(pos.z());
    AdapterManager::FillRoutingRequestHeader(kHMIRosBridgeName,
                                             &routing_request);
    AdapterManager::PublishRoutingRequest(routing_request);
  }

 private:
  routing::RoutingRequest routing_request_template;

  DECLARE_SINGLETON(RosBridge);
};

RosBridge::RosBridge() {}

}  // namespace
}  // namespace hmi
}  // namespace apollo

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  ros::init(argc, argv, apollo::hmi::kHMIRosBridgeName);
  apollo::hmi::RosBridge::instance()->Init();
  ros::spin();
  return 0;
}
