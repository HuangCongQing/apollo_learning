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
 * @file path_optimizer.cpp
 **/

#include "modules/planning/optimizer/path_optimizer.h"

namespace apollo {
namespace planning {

PathOptimizer::PathOptimizer(const std::string& name) : Optimizer(name) {}

apollo::common::Status PathOptimizer::Optimize(Frame* frame) {
  frame_ = frame;
  auto* planning_data = frame->mutable_planning_data();
  auto ret = Process(planning_data->speed_data(), frame->reference_line(),
                     frame->PlanningStartPoint(), frame->path_decision(),
                     planning_data->mutable_path_data());
  RecordDebugInfo(planning_data->path_data());

  return ret;
}

void PathOptimizer::RecordDebugInfo(const PathData& path_data) {
  auto debug = frame_->MutableADCTrajectory()->mutable_debug();
  const auto& path_points = path_data.discretized_path().path_points();
  auto ptr_optimized_path = debug->mutable_planning_data()->add_path();
  ptr_optimized_path->set_name(name());
  ptr_optimized_path->mutable_path_point()->CopyFrom(
      {path_points.begin(), path_points.end()});
}

}  // namespace planning
}  // namespace apollo
