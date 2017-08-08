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
 * @file frame.cc
 **/
#include "modules/planning/common/frame.h"

#include <cmath>
#include <string>

#include "modules/common/adapters/adapter_manager.h"
#include "modules/common/log.h"
#include "modules/map/pnc_map/pnc_map.h"
#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/reference_line/reference_line_smoother.h"

namespace apollo {
namespace planning {

using apollo::common::adapter::AdapterManager;

const hdmap::PncMap *Frame::pnc_map_ = nullptr;

void Frame::SetMap(hdmap::PncMap *pnc_map) { pnc_map_ = pnc_map; }

FrameHistory::FrameHistory()
    : IndexedQueue<uint32_t, Frame>(FLAGS_max_history_frame_num) {}

Frame::Frame(const uint32_t sequence_num) : sequence_num_(sequence_num) {
  DCHECK(common::util::GetProtoFromFile(
      FLAGS_reference_line_smoother_config_file, &smoother_config_))
      << "Failed to init reference line smoother config with file "
      << FLAGS_reference_line_smoother_config_file;
}

void Frame::SetVehicleInitPose(const localization::Pose &pose) {
  init_pose_ = pose;
}

const localization::Pose &Frame::VehicleInitPose() const { return init_pose_; }

void Frame::SetRoutingResponse(const routing::RoutingResponse &routing) {
  routing_response_ = routing;
}

void Frame::SetPlanningStartPoint(const common::TrajectoryPoint &start_point) {
  planning_start_point_ = start_point;
}

const hdmap::PncMap *Frame::PncMap() {
  DCHECK(pnc_map_) << "map is not setup in frame";
  return pnc_map_;
}

const common::TrajectoryPoint &Frame::PlanningStartPoint() const {
  return planning_start_point_;
}

void Frame::SetPrediction(const prediction::PredictionObstacles &prediction) {
  prediction_ = prediction;
}

void Frame::CreatePredictionObstacles(
    const prediction::PredictionObstacles &prediction) {
  std::list<std::unique_ptr<Obstacle> > obstacles;
  Obstacle::CreateObstacles(prediction, &obstacles);
  for (auto &ptr : obstacles) {
    auto id(ptr->Id());
    obstacles_.Add(id, std::move(ptr));
  }
}

const ADCTrajectory &Frame::GetADCTrajectory() const { return trajectory_pb_; }

ADCTrajectory *Frame::MutableADCTrajectory() { return &trajectory_pb_; }

PathDecision *Frame::path_decision() { return path_decision_.get(); }

bool Frame::Init() {
  if (!pnc_map_) {
    AERROR << "map is null, call SetMap() first";
    return false;
  }
  const auto &point = init_pose_.position();
  if (std::isnan(point.x()) || std::isnan(point.y())) {
    AERROR << "init point is not set";
    return false;
  }
  std::vector<ReferenceLine> reference_lines;
  if (!CreateReferenceLineFromRouting(init_pose_.position(), routing_response_,
                                      &reference_lines)) {
    AERROR << "Failed to create reference line from position: "
           << init_pose_.DebugString();
    return false;
  }
  // TODO add fuctions to help select the best reference_line(s)
  reference_line_ = reference_lines.front();

  if (FLAGS_enable_prediction) {
    CreatePredictionObstacles(prediction_);
  }

  path_decision_ = common::util::make_unique<PathDecision>(obstacles_.Items(),
                                                           reference_line_);

  if (FLAGS_enable_traffic_decision) {
    MakeTrafficDecision(routing_response_, reference_line_);
  }

  return true;
}

uint32_t Frame::sequence_num() const { return sequence_num_; }

const PlanningData &Frame::planning_data() const { return _planning_data; }

PlanningData *Frame::mutable_planning_data() { return &_planning_data; }

void Frame::set_computed_trajectory(const PublishableTrajectory &trajectory) {
  _computed_trajectory = trajectory;
}

const PublishableTrajectory &Frame::computed_trajectory() const {
  return _computed_trajectory;
}

const ReferenceLine &Frame::reference_line() const { return reference_line_; }

bool Frame::MakeTrafficDecision(const routing::RoutingResponse &,
                                const ReferenceLine &) {
  return true;
}

bool Frame::CreateReferenceLineFromRouting(
    const common::PointENU &position, const routing::RoutingResponse &routing,
    std::vector<ReferenceLine> *reference_lines) {
  CHECK_NOTNULL(reference_lines);

  hdmap::Path hdmap_path;
  if (!pnc_map_->CreatePathFromRouting(
          routing, position, FLAGS_look_backward_distance,
          FLAGS_look_forward_distance, &hdmap_path)) {
    AERROR << "Failed to get path from routing";
    return false;
  }
  reference_lines->emplace_back(ReferenceLine());
  ReferenceLineSmoother smoother;
  smoother.Init(smoother_config_);
  if (!smoother.smooth(ReferenceLine(hdmap_path), &reference_lines->back())) {
    AERROR << "Failed to smooth reference line";
    return false;
  }
  return true;
}

const IndexedObstacles &Frame::GetObstacles() const { return obstacles_; }

std::string Frame::DebugString() const {
  return "Frame: " + std::to_string(sequence_num_);
}

void Frame::RecordInputDebug() {
  if (!FLAGS_enable_record_debug) {
    ADEBUG << "Skip record input into debug";
    return;
  }
  auto planning_data = trajectory_pb_.mutable_debug()->mutable_planning_data();
  auto adc_position = planning_data->mutable_adc_position();
  const auto &localization =
      AdapterManager::GetLocalization()->GetLatestObserved();
  adc_position->CopyFrom(localization);

  const auto &chassis = AdapterManager::GetChassis()->GetLatestObserved();
  auto debug_chassis = planning_data->mutable_chassis();
  debug_chassis->CopyFrom(chassis);

  const auto &routing_response =
      AdapterManager::GetRoutingResponse()->GetLatestObserved();

  auto debug_routing = planning_data->mutable_routing();
  debug_routing->CopyFrom(routing_response);
}

void Frame::AlignPredictionTime(const double trajectory_header_time) {
  double prediction_header_time = prediction_.header().timestamp_sec();

  for (int i = 0; i < prediction_.prediction_obstacle_size(); i++) {
    prediction::PredictionObstacle* obstacle =
        prediction_.mutable_prediction_obstacle(i);
    for (int j = 0; j < obstacle->trajectory_size(); j++) {
      prediction::Trajectory* trajectory = obstacle->mutable_trajectory(j);
      for (int k = 0; k < trajectory->trajectory_point_size(); k++) {
        common::TrajectoryPoint* point
            = trajectory->mutable_trajectory_point(k);
        double abs_relative_time = point->relative_time();
        point->set_relative_time(prediction_header_time
                                     + abs_relative_time
                                     - trajectory_header_time);
      }
    }
  }
}

}  // namespace planning
}  // namespace apollo
