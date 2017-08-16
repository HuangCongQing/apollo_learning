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

/**
 * @file decider.cpp
 **/

#include <limits>

#include "modules/common/configs/vehicle_config_helper.h"
#include "modules/common/log.h"
#include "modules/common/vehicle_state/vehicle_state.h"
#include "modules/planning/planner/em/decider.h"

namespace apollo {
namespace planning {

using common::ErrorCode;
using common::Status;
using common::VehicleState;

Decider::Decider(DecisionResult* decision_result)
    : decision_(decision_result) {}

const DecisionResult& Decider::Decision() const { return *decision_; }

Status Decider::MakeDecision(Frame* frame) {
  decision_->Clear();

  auto path_decision = frame->path_decision();

  bool estop = 0;
  if (estop) {
    MakeEStopDecision(path_decision);
    return Status(ErrorCode::OK, "estop");
  }

  // cruise by default
  decision_->mutable_main_decision()->mutable_cruise();

  // check stop decision
  int error_code = MakeMainStopDecision(frame, path_decision);
  if (error_code < 0) {
    MakeEStopDecision(path_decision);
    return Status(ErrorCode::OK, "MakeDecision failed. estop.");
  } else if (error_code == 0) {
    // TODO(all): check other main decisions
  }

  SetObjectDecisions(path_decision);
  return Status(ErrorCode::OK, "MakeDecision completed");
}

int Decider::MakeMainStopDecision(Frame* frame,
                                  PathDecision* const path_decision) {
  if (!frame) {
    AERROR << "Frame is empty in Decider";
    return 0;
  }

  CHECK_NOTNULL(path_decision);

  double min_stop_line_s = std::numeric_limits<double>::infinity();
  const Obstacle* stop_obstacle = nullptr;
  const ObjectStop* stop_decision = nullptr;

  const auto& path_obstacles = path_decision->path_obstacles();
  for (const auto path_obstacle : path_obstacles.Items()) {
    const auto& obstacle = path_obstacle->Obstacle();
    const auto& object_decision = path_obstacle->LongitudinalDecision();
    if (!object_decision.has_stop()) {
      continue;
    }

    const auto& reference_line = frame->reference_line();
    apollo::common::PointENU stop_point = object_decision.stop().stop_point();
    common::SLPoint stop_line_sl;
    reference_line.get_point_in_frenet_frame({stop_point.x(), stop_point.y()},
                                             &stop_line_sl);

    double stop_line_s = stop_line_sl.s();
    if (stop_line_s < 0 || stop_line_s > reference_line.length()) {
      AERROR << "Ignore object:" << obstacle->Id() << " fence route_s["
             << stop_line_s << "] not in range[0, " << reference_line.length()
             << "]";
      continue;
    }

    // check stop_line_s vs adc_s
    common::SLPoint adc_sl;
    auto& adc_position = VehicleState::instance()->pose().position();
    reference_line.get_point_in_frenet_frame(
        {adc_position.x(), adc_position.y()}, &adc_sl);
    const auto& vehicle_param =
        common::VehicleConfigHelper::instance()->GetConfig().vehicle_param();
    if (stop_line_s <= adc_sl.s() + vehicle_param.front_edge_to_center()) {
      AERROR << "object:" << obstacle->Id() << " fence route_s[" << stop_line_s
             << "] behind adc route_s[" << adc_sl.s() << "]";
      continue;
    }

    if (stop_line_s < min_stop_line_s) {
      min_stop_line_s = stop_line_s;
      stop_obstacle = obstacle;
      stop_decision = &(object_decision.stop());
    }
  }

  if (stop_obstacle != nullptr) {
    MainStop* main_stop = decision_->mutable_main_decision()->mutable_stop();
    main_stop->set_reason_code(stop_decision->reason_code());
    main_stop->set_reason("stop by " + stop_obstacle->Id());
    main_stop->mutable_stop_point()->set_x(stop_decision->stop_point().x());
    main_stop->mutable_stop_point()->set_y(stop_decision->stop_point().y());
    main_stop->set_stop_heading(stop_decision->stop_heading());

    ADEBUG << " main stop obstacle id:" << stop_obstacle->Id()
           << " stop_line_s:" << min_stop_line_s << " stop_point: ("
           << stop_decision->stop_point().x() << stop_decision->stop_point().y()
           << " ) stop_heading: " << stop_decision->stop_heading();

    return 1;
  }

  return 0;
}

int Decider::SetObjectDecisions(PathDecision* const path_decision) {
  ObjectDecisions* object_decisions = decision_->mutable_object_decision();

  const auto& path_obstacles = path_decision->path_obstacles();
  for (const auto path_obstacle : path_obstacles.Items()) {
    auto* object_decision = object_decisions->add_decision();

    const auto& obstacle = path_obstacle->Obstacle();
    object_decision->set_id(obstacle->Id());
    object_decision->set_perception_id(obstacle->PerceptionId());
    if (path_obstacle->IsIgnore()) {
      object_decision->add_object_decision()->mutable_ignore();
      continue;
    }
    if (path_obstacle->HasLateralDecision()) {
      object_decision->add_object_decision()->CopyFrom(
          path_obstacle->LateralDecision());
    }
    if (path_obstacle->HasLongitudinalDecision()) {
      object_decision->add_object_decision()->CopyFrom(
          path_obstacle->LongitudinalDecision());
    }
  }
  return 0;
}

int Decider::MakeEStopDecision(PathDecision* const path_decision) {
  CHECK_NOTNULL(path_decision);
  decision_->Clear();

  // main decision
  // TODO(all): to be added
  MainEmergencyStop* main_estop =
      decision_->mutable_main_decision()->mutable_estop();
  main_estop->set_reason_code(MainEmergencyStop::ESTOP_REASON_INTERNAL_ERR);
  main_estop->set_reason("estop reason to be added");
  main_estop->mutable_cruise_to_stop();

  // set object decisions
  ObjectDecisions* object_decisions = decision_->mutable_object_decision();
  const auto& path_obstacles = path_decision->path_obstacles();
  for (const auto path_obstacle : path_obstacles.Items()) {
    auto* object_decision = object_decisions->add_decision();
    const auto& obstacle = path_obstacle->Obstacle();
    object_decision->set_id(obstacle->Id());
    object_decision->set_perception_id(obstacle->PerceptionId());
    object_decision->add_object_decision()->mutable_avoid();
  }
  return 0;
}

}  // namespace planning
}  // namespace apollo
