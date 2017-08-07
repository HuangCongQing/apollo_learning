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
*   @file
**/

#ifndef MODULES_PLANNING_OPTIMIZER_ST_GRAPH_ST_BOUNDARY_MAPPER_H_
#define MODULES_PLANNING_OPTIMIZER_ST_GRAPH_ST_BOUNDARY_MAPPER_H_

#include <vector>

#include "modules/common/configs/proto/vehicle_config.pb.h"
#include "modules/common/status/status.h"
#include "modules/planning/proto/st_boundary_config.pb.h"

#include "modules/planning/common/path/path_data.h"
#include "modules/planning/common/path_decision.h"
#include "modules/planning/common/speed_limit.h"
#include "modules/planning/optimizer/st_graph/st_graph_boundary.h"
#include "modules/planning/reference_line/reference_line.h"

namespace apollo {
namespace planning {

class StBoundaryMapper {
 public:
  StBoundaryMapper(const StBoundaryConfig& config,
                   const ReferenceLine& reference_line,
                   const PathData& path_data, const double planning_distance,
                   const double planning_time);

  apollo::common::Status GetGraphBoundary(
      const PathDecision& path_decision,
      std::vector<StGraphBoundary>* const boundary) const;

  virtual apollo::common::Status GetSpeedLimits(
      SpeedLimit* const speed_limit_data) const;

 private:
  bool CheckOverlap(const apollo::common::PathPoint& path_point,
                    const apollo::common::math::Box2d& obs_box,
                    const double buffer) const;

  double GetArea(const std::vector<STPoint>& boundary_points) const;

  bool MapObstacleWithStopDecision(const PathObstacle& stop_obstacle,
                                   const ObjectDecisionType& stop_decision,
                                   StGraphBoundary* const boundary) const;

  apollo::common::Status MapObstacleWithPredictionTrajectory(
      const PathObstacle& path_obstacle, const ObjectDecisionType& obj_decision,
      std::vector<StGraphBoundary>* const boundary) const;

  apollo::common::Status MapFollowDecision(
      const PathObstacle& obstacle, const ObjectDecisionType& obj_decision,
      StGraphBoundary* const boundary) const;

 private:
  StBoundaryConfig st_boundary_config_;
  const ReferenceLine& reference_line_;
  const PathData& path_data_;
  const apollo::common::VehicleParam vehicle_param_;
  const double planning_distance_;
  const double planning_time_;
  double adc_front_s_ = 0.0;
};

}  // namespace planning
}  // namespace apollo

#endif  // MODULES_PLANNING_OPTIMIZER_ST_GRAPH_ST_BOUNDARY_MAPPER_H_
