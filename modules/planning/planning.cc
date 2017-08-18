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

#include "modules/planning/planning.h"

#include <algorithm>

#include "google/protobuf/repeated_field.h"
#include "modules/common/adapters/adapter_manager.h"
#include "modules/common/time/time.h"
#include "modules/common/vehicle_state/vehicle_state.h"
#include "modules/map/hdmap/hdmap_util.h"
#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/planner/em/em_planner.h"
#include "modules/planning/planner/rtk/rtk_replay_planner.h"
#include "modules/planning/trajectory_stitcher/trajectory_stitcher.h"

namespace apollo {
namespace planning {

using apollo::common::ErrorCode;
using apollo::common::Status;
using apollo::common::TrajectoryPoint;
using apollo::common::VehicleState;
using apollo::common::adapter::AdapterManager;
using apollo::common::time::Clock;

std::string Planning::Name() const { return "planning"; }

void Planning::RegisterPlanners() {
  planner_factory_.Register(
      PlanningConfig::RTK, []() -> Planner* { return new RTKReplayPlanner(); });
  planner_factory_.Register(PlanningConfig::EM,
                            []() -> Planner* { return new EMPlanner(); });
}

const Frame* Planning::GetFrame() const { return frame_.get(); }
const hdmap::PncMap* Planning::GetPncMap() const { return pnc_map_.get(); }

bool Planning::InitFrame(const uint32_t sequence_num) {
  frame_.reset(new Frame(sequence_num));
  if (AdapterManager::GetRoutingResponse()->Empty()) {
    AERROR << "Routing is empty";
    return false;
  }
  common::TrajectoryPoint point;
  frame_->SetVehicleInitPose(VehicleState::instance()->pose());
  frame_->SetRoutingResponse(
      AdapterManager::GetRoutingResponse()->GetLatestObserved());
  if (FLAGS_enable_prediction && !AdapterManager::GetPrediction()->Empty()) {
    const auto& prediction =
        AdapterManager::GetPrediction()->GetLatestObserved();
    frame_->SetPrediction(prediction);
    ADEBUG << "Get prediction";
  }

  if (!frame_->Init(config_)) {
    AERROR << "failed to init frame";
    return false;
  }
  frame_->RecordInputDebug();
  return true;
}

void Planning::SetConfig(const PlanningConfig& config) { config_ = config; }

Status Planning::Init() {
  pnc_map_.reset(new hdmap::PncMap(apollo::hdmap::BaseMapFile()));
  Frame::SetMap(pnc_map_.get());

  if (!apollo::common::util::GetProtoFromFile(FLAGS_planning_config_file,
                                              &config_)) {
    AERROR << "failed to load planning config file "
           << FLAGS_planning_config_file;
    return Status(
        ErrorCode::PLANNING_ERROR,
        "failed to load planning config file: " + FLAGS_planning_config_file);
  }

  if (!AdapterManager::Initialized()) {
    AdapterManager::Init(FLAGS_adapter_config_path);
  }
  if (AdapterManager::GetLocalization() == nullptr) {
    std::string error_msg("Localization is not registered");
    AERROR << error_msg;
    return Status(ErrorCode::PLANNING_ERROR, error_msg);
  }
  if (AdapterManager::GetChassis() == nullptr) {
    std::string error_msg("Chassis is not registered");
    AERROR << error_msg;
    return Status(ErrorCode::PLANNING_ERROR, error_msg);
  }
  if (AdapterManager::GetRoutingResponse() == nullptr) {
    std::string error_msg("RoutingResponse is not registered");
    AERROR << error_msg;
    return Status(ErrorCode::PLANNING_ERROR, error_msg);
  }
  // TODO(all) temporarily use the offline routing data.
  if (!AdapterManager::GetRoutingResponse()->HasReceived()) {
    if (!AdapterManager::GetRoutingResponse()->FeedFile(
            FLAGS_offline_routing_file)) {
      auto error_msg = common::util::StrCat(
          "Failed to load offline routing file ", FLAGS_offline_routing_file);
      AERROR << error_msg;
      return Status(ErrorCode::PLANNING_ERROR, error_msg);
    } else {
      AWARN << "Using offline routing file " << FLAGS_offline_routing_file;
    }
  }
  if (AdapterManager::GetPrediction() == nullptr) {
    std::string error_msg("Prediction is not registered");
    AERROR << error_msg;
    return Status(ErrorCode::PLANNING_ERROR, error_msg);
  }

  RegisterPlanners();
  planner_ = planner_factory_.CreateObject(config_.planner_type());
  if (!planner_) {
    return Status(
        ErrorCode::PLANNING_ERROR,
        "planning is not initialized with config : " + config_.DebugString());
  }

  return planner_->Init(config_);
}

Status Planning::Start() {
  static ros::Rate loop_rate(FLAGS_planning_loop_rate);
  while (ros::ok()) {
    RunOnce();
    if (frame_) {
      auto seq_num = frame_->sequence_num();
      FrameHistory::instance()->Add(seq_num, std::move(frame_));
    }
    ros::spinOnce();
    loop_rate.sleep();
  }
  return Status::OK();
}

void Planning::PublishPlanningPb(ADCTrajectory* trajectory_pb) {
  AdapterManager::FillPlanningHeader("planning", trajectory_pb);
  // TODO(all): integrate reverse gear
  trajectory_pb->set_gear(canbus::Chassis::GEAR_DRIVE);
  AdapterManager::PublishPlanning(*trajectory_pb);
}

void Planning::PublishPlanningPb(ADCTrajectory* trajectory_pb,
                                 double timestamp) {
  AdapterManager::FillPlanningHeader("planning", trajectory_pb);
  trajectory_pb->mutable_header()->set_timestamp_sec(timestamp);
  // TODO(all): integrate reverse gear
  trajectory_pb->set_gear(canbus::Chassis::GEAR_DRIVE);
  AdapterManager::PublishPlanning(*trajectory_pb);
}

void Planning::RunOnce() {
  AdapterManager::Observe();
  ADCTrajectory not_ready_pb;
  auto* not_ready = not_ready_pb.mutable_decision()
                        ->mutable_main_decision()
                        ->mutable_not_ready();
  if (AdapterManager::GetLocalization()->Empty()) {
    AERROR << "Localization is not available; skip the planning cycle";
    not_ready->set_reason("localization not ready");
    PublishPlanningPb(&not_ready_pb);
    return;
  }

  if (AdapterManager::GetChassis()->Empty()) {
    AERROR << "Chassis is not available; skip the planning cycle";
    not_ready->set_reason("chassis not ready");
    PublishPlanningPb(&not_ready_pb);
    return;
  }
  if (AdapterManager::GetRoutingResponse()->Empty()) {
    AERROR << "RoutingResponse is not available; skip the planning cycle";
    not_ready->set_reason("routing not ready");
    PublishPlanningPb(&not_ready_pb);
    return;
  }

  if (FLAGS_enable_prediction && AdapterManager::GetPrediction()->Empty()) {
    AERROR << "Prediction is not available; skip the planning cycle";
    not_ready->set_reason("prediction not ready");
    PublishPlanningPb(&not_ready_pb);
    return;
  }

  const double start_timestamp = apollo::common::time::ToSecond(Clock::Now());

  // localization
  const auto& localization =
      AdapterManager::GetLocalization()->GetLatestObserved();
  ADEBUG << "Get localization:" << localization.DebugString();

  // chassis
  const auto& chassis = AdapterManager::GetChassis()->GetLatestObserved();
  ADEBUG << "Get chassis:" << chassis.DebugString();

  common::VehicleState::instance()->Update(localization, chassis);

  const double planning_cycle_time = 1.0 / FLAGS_planning_loop_rate;

  const uint32_t frame_num = AdapterManager::GetPlanning()->GetSeqNum() + 1;
  if (!InitFrame(frame_num)) {
    AERROR << "Init frame failed";
    return;
  }

  bool is_auto_mode = chassis.driving_mode() == chassis.COMPLETE_AUTO_DRIVE;
  bool res_planning = Plan(is_auto_mode, start_timestamp, planning_cycle_time);

  const double end_timestamp = apollo::common::time::ToSecond(Clock::Now());
  const double time_diff_ms = (end_timestamp - start_timestamp) * 1000;
  auto trajectory_pb = frame_->MutableADCTrajectory();
  trajectory_pb->mutable_latency_stats()->set_total_time_ms(time_diff_ms);
  ADEBUG << "Planning latency: "
         << trajectory_pb->latency_stats().DebugString();

  if (res_planning) {
    PublishPlanningPb(trajectory_pb, start_timestamp);
    ADEBUG << "Planning succeeded:" << trajectory_pb->header().DebugString();
  } else {
    AERROR << "Planning failed";
  }
}

void Planning::Stop() {}

bool Planning::Plan(const bool is_on_auto_mode, const double current_time_stamp,
                    const double planning_cycle_time) {
  const auto& stitching_trajectory =
      TrajectoryStitcher::ComputeStitchingTrajectory(
          is_on_auto_mode, current_time_stamp, planning_cycle_time,
          last_publishable_trajectory_);

  frame_->SetPlanningStartPoint(stitching_trajectory.back());

  auto trajectory_pb = frame_->MutableADCTrajectory();
  if (FLAGS_enable_record_debug) {
    trajectory_pb->mutable_debug()
        ->mutable_planning_data()
        ->mutable_init_point()
        ->CopyFrom(stitching_trajectory.back());
  }

  frame_->AlignPredictionTime(current_time_stamp);

  ReferenceLineInfo* best_reference_line = nullptr;
  for (auto& reference_line_info : frame_->reference_line_info()) {
    auto status = planner_->Plan(stitching_trajectory.back(), frame_.get(),
                                 &reference_line_info);
    if (status != Status::OK()) {
      AERROR << "planner failed to make a driving plan";
      continue;
    }
    // FIXME: compare reference_lines;
    best_reference_line = &reference_line_info;
  }
  if (!best_reference_line) {
    AERROR << "planner failed to make a driving plan";
    last_publishable_trajectory_.Clear();
    return false;
  }

  PublishableTrajectory publishable_trajectory(
      current_time_stamp, best_reference_line->trajectory());

  publishable_trajectory.PrependTrajectoryPoints(
      stitching_trajectory.begin(), stitching_trajectory.end() - 1);

  publishable_trajectory.set_header_time(current_time_stamp);

  publishable_trajectory.PopulateTrajectoryProtobuf(trajectory_pb);
  trajectory_pb->set_is_replan(stitching_trajectory.size() == 1);

  // update last publishable trajectory;
  last_publishable_trajectory_ = std::move(publishable_trajectory);

  return true;
}

}  // namespace planning
}  // namespace apollo
