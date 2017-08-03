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

#include "modules/dreamview/backend/sim_control/sim_control.h"

#include <cmath>

#include "modules/common/math/math_utils.h"
#include "modules/common/math/quaternion.h"
#include "modules/common/time/time.h"
#include "modules/common/util/file.h"

namespace apollo {
namespace dreamview {

using apollo::common::adapter::AdapterManager;
using apollo::common::math::HeadingToQuaternion;
using apollo::common::math::QuaternionToHeading;
using apollo::common::math::NormalizeAngle;
using apollo::common::time::Clock;
using apollo::common::TrajectoryPoint;
using apollo::common::util::GetProtoFromFile;
using apollo::routing::RoutingResponse;
using apollo::localization::LocalizationEstimate;
using apollo::canbus::Chassis;

SimControl::SimControl()
    : prev_point_index_(0),
      next_point_index_(0),
      received_planning_(false),
      initial_start_(true),
      enabled_(FLAGS_enable_sim_control) {
  if (enabled_) {
    RoutingResponse routing;
    if (!GetProtoFromFile(FLAGS_routing_result_file, &routing)) {
      AWARN << "Unable to read start point from file: "
            << FLAGS_routing_result_file;
      return;
    }
    SetStartPoint(routing);
  }
}

void SimControl::SetStartPoint(const RoutingResponse& routing) {
  next_point_.set_v(0.0);
  next_point_.set_a(0.0);

  auto* p = next_point_.mutable_path_point();

  p->set_x(routing.routing_request().start().pose().x());
  p->set_y(routing.routing_request().start().pose().y());
  p->set_z(0.0);

  // TODO(siyangy): Calculate the real theta when the API is ready.
  p->set_theta(0.0);
  p->set_kappa(0.0);
  p->set_s(0.0);

  prev_point_index_ = next_point_index_ = 0;
  received_planning_ = false;

  if (enabled_) {
    Start();
  }
}

void SimControl::Start() {
  if (initial_start_) {
    // Setup planning and routing result data callback.
    AdapterManager::AddPlanningCallback(&SimControl::OnPlanning, this);
    AdapterManager::AddRoutingResponseCallback(&SimControl::SetStartPoint, this);

    // Start timer to publish localiztion and chassis messages.
    sim_control_timer_ = AdapterManager::CreateTimer(
        ros::Duration(kSimControlInterval), &SimControl::TimerCallback, this);

    initial_start_ = false;
  } else {
    sim_control_timer_.start();
  }
}

void SimControl::Stop() {
  sim_control_timer_.stop();
}

void SimControl::OnPlanning(const apollo::planning::ADCTrajectory& trajectory) {
  // Reset current trajectory and the indices upon receiving a new trajectory.
  current_trajectory_ = trajectory;
  prev_point_index_ = 0;
  next_point_index_ = 0;
  received_planning_ = true;
}

void SimControl::Freeze() {
  next_point_.set_v(0.0);
  next_point_.set_a(0.0);
  prev_point_ = next_point_;
}

double SimControl::AbsoluteTimeOfNextPoint() {
  return current_trajectory_.header().timestamp_sec() +
         current_trajectory_.trajectory_point(next_point_index_)
             .relative_time();
}

bool SimControl::NextPointWithinRange() {
  return next_point_index_ < current_trajectory_.trajectory_point_size() - 1;
}

void SimControl::TimerCallback(const ros::TimerEvent& event) {
  // Result of the interpolation.
  double lambda = 0;
  auto current_time = apollo::common::time::ToSecond(Clock::Now());

  if (!received_planning_) {
    prev_point_ = next_point_;
  } else {
    if (current_trajectory_.estop().is_estop() || !NextPointWithinRange()) {
      // Freeze the car when there's an estop or the current trajectory has been
      // exhausted.
      Freeze();
    } else {
      // Determine the status of the car based on received planning message.
      double timestamp = current_trajectory_.header().timestamp_sec();

      while (NextPointWithinRange() &&
             current_time > AbsoluteTimeOfNextPoint()) {
        ++next_point_index_;
      }

      if (next_point_index_ == 0) {
        AERROR << "First trajectory point is a future point!";
        return;
      }

      if (current_time > AbsoluteTimeOfNextPoint()) {
        prev_point_index_ = next_point_index_;
      } else {
        prev_point_index_ = next_point_index_ - 1;
      }

      next_point_ = current_trajectory_.trajectory_point(next_point_index_);
      prev_point_ = current_trajectory_.trajectory_point(prev_point_index_);

      // Calculate the ratio based on the the position of current time in
      // between the previous point and the next point, where lamda =
      // (current_point - prev_point) / (next_point - prev_point).
      if (next_point_index_ != prev_point_index_) {
        lambda = (current_time - timestamp - prev_point_.relative_time()) /
                 (next_point_.relative_time() - prev_point_.relative_time());
      }
    }
  }

  PublishChassis(lambda);
  PublishLocalization(lambda);
}

void SimControl::PublishChassis(double lambda) {
  Chassis chassis;
  AdapterManager::FillChassisHeader("SimControl", &chassis);

  chassis.set_engine_started(true);
  chassis.set_driving_mode(Chassis::COMPLETE_AUTO_DRIVE);
  chassis.set_gear_location(Chassis::GEAR_DRIVE);

  double cur_speed = Interpolate(prev_point_.v(), next_point_.v(), lambda);
  chassis.set_speed_mps(cur_speed);

  AdapterManager::PublishChassis(chassis);
}

void SimControl::PublishLocalization(double lambda) {
  LocalizationEstimate localization;
  AdapterManager::FillLocalizationHeader("SimControl", &localization);

  auto* pose = localization.mutable_pose();
  auto prev = prev_point_.path_point();
  auto next = next_point_.path_point();

  // Set position
  double cur_x = Interpolate(prev.x(), next.x(), lambda);
  pose->mutable_position()->set_x(cur_x);
  double cur_y = Interpolate(prev.y(), next.y(), lambda);
  pose->mutable_position()->set_y(cur_y);
  double cur_z = Interpolate(prev.z(), next.z(), lambda);
  pose->mutable_position()->set_z(cur_z);

  // Set orientation and heading
  double cur_theta = NormalizeAngle(
      prev.theta() + lambda * NormalizeAngle(next.theta() - prev.theta()));

  Eigen::Quaternion<double> cur_orientation =
      HeadingToQuaternion<double>(cur_theta);
  pose->mutable_orientation()->set_qw(cur_orientation.w());
  pose->mutable_orientation()->set_qx(cur_orientation.x());
  pose->mutable_orientation()->set_qy(cur_orientation.y());
  pose->mutable_orientation()->set_qz(cur_orientation.z());
  pose->set_heading(cur_theta);

  // Set linear_velocity
  double cur_speed = Interpolate(prev_point_.v(), next_point_.v(), lambda);
  pose->mutable_linear_velocity()->set_x(std::cos(cur_theta) * cur_speed);
  pose->mutable_linear_velocity()->set_y(std::sin(cur_theta) * cur_speed);
  pose->mutable_linear_velocity()->set_z(0);

  // Set angular_velocity
  double cur_curvature = Interpolate(prev.kappa(), next.kappa(), lambda);
  pose->mutable_angular_velocity()->set_x(0);
  pose->mutable_angular_velocity()->set_y(0);
  pose->mutable_angular_velocity()->set_z(cur_speed * cur_curvature);

  // Set linear_acceleration
  double cur_acceleration_s =
      Interpolate(prev_point_.a(), next_point_.a(), lambda);
  auto* linear_acceleration = pose->mutable_linear_acceleration();
  linear_acceleration->set_x(std::cos(cur_theta) * cur_acceleration_s);
  linear_acceleration->set_y(std::sin(cur_theta) * cur_acceleration_s);
  linear_acceleration->set_z(0);

  AdapterManager::PublishLocalization(localization);
}

}  // namespace dreamview
}  // namespace apollo
