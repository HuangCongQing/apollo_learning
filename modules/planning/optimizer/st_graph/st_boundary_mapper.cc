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
 * @file
 **/

#include "modules/planning/optimizer/st_graph/st_boundary_mapper.h"

#include <algorithm>
#include <limits>
#include <string>

#include "modules/common/proto/pnc_point.pb.h"
#include "modules/planning/proto/decision.pb.h"

#include "modules/common/configs/vehicle_config_helper.h"
#include "modules/common/log.h"
#include "modules/common/math/line_segment2d.h"
#include "modules/common/math/vec2d.h"
#include "modules/common/util/file.h"
#include "modules/common/util/string_util.h"
#include "modules/common/util/util.h"
#include "modules/common/vehicle_state/vehicle_state.h"
#include "modules/planning/common/frame.h"
#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/math/double.h"

namespace apollo {
namespace planning {

using ErrorCode = apollo::common::ErrorCode;
using Status = apollo::common::Status;
using PathPoint = apollo::common::PathPoint;
using TrajectoryPoint = apollo::common::TrajectoryPoint;
using SLPoint = apollo::common::SLPoint;
using VehicleParam = apollo::common::VehicleParam;
using Box2d = apollo::common::math::Box2d;
using Vec2d = apollo::common::math::Vec2d;

StBoundaryMapper::StBoundaryMapper(const StBoundaryConfig& config,
                                   const ReferenceLine& reference_line,
                                   const PathData& path_data,
                                   const double planning_distance,
                                   const double planning_time)
    : st_boundary_config_(config),
      reference_line_(reference_line),
      path_data_(path_data),
      vehicle_param_(
          common::VehicleConfigHelper::instance()->GetConfig().vehicle_param()),
      planning_distance_(planning_distance),
      planning_time_(planning_time) {
  const auto& path_start_point = path_data_.discretized_path().StartPoint();
  common::SLPoint sl_point;
  DCHECK(reference_line_.get_point_in_frenet_frame(
      {path_start_point.x(), path_start_point.y()}, &sl_point))
      << "Failed to get adc reference line s";
  adc_front_s_ = sl_point.s() + vehicle_param_.front_edge_to_center();
}

Status StBoundaryMapper::GetGraphBoundary(
    const PathDecision& path_decision,
    std::vector<StGraphBoundary>* const st_graph_boundaries) const {
  const auto& path_obstacles = path_decision.path_obstacles();
  if (st_graph_boundaries == nullptr) {
    const std::string msg = "st_graph_boundaries is NULL.";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }

  if (planning_time_ < 0.0) {
    const std::string msg = "Fail to get params since planning_time_ < 0.";
    AERROR << msg;
    return Status(ErrorCode::PLANNING_ERROR, msg);
  }

  if (path_data_.discretized_path().NumOfPoints() < 2) {
    AERROR << "Fail to get params because of too few path points. path points "
              "size: "
           << path_data_.discretized_path().NumOfPoints() << ".";
    return Status(ErrorCode::PLANNING_ERROR,
                  "Fail to get params because of too few path points");
  }

  st_graph_boundaries->clear();
  Status ret = Status::OK();

  const PathObstacle* stop_obstacle = nullptr;
  ObjectDecisionType stop_decision;
  double min_stop_s = std::numeric_limits<double>::max();

  for (const auto* path_obstacle : path_obstacles.Items()) {
    if (!path_obstacle->HasLongitudinalDecision()) {
      StGraphBoundary boundary;
      const auto ret = MapWithoutDecision(*path_obstacle, &boundary);
      if (!ret.ok()) {
        std::string msg = common::util::StrCat(
            "Fail to map obstacle ", path_obstacle->Id(), " without decision.");
        AERROR << msg;
        return Status(ErrorCode::PLANNING_ERROR, msg);
      }
      AppendBoundary(boundary, st_graph_boundaries);
      continue;
    }
    const auto& decision = path_obstacle->LongitudinalDecision();
    if (decision.has_follow()) {
      StGraphBoundary follow_boundary;
      const auto ret =
          MapFollowDecision(*path_obstacle, decision, &follow_boundary);
      if (!ret.ok()) {
        AERROR << "Fail to map obstacle " << path_obstacle->Id()
               << " with follow decision: " << decision.DebugString();
        return Status(ErrorCode::PLANNING_ERROR, "Fail to map follow decision");
      }
      AppendBoundary(follow_boundary, st_graph_boundaries);
    } else if (decision.has_stop()) {
      // TODO(all) change start_s() to st_boundary.min_s()
      const double stop_s = path_obstacle->perception_sl_boundary().start_s() +
                            decision.stop().distance_s();
      if (stop_s < adc_front_s_) {
        AERROR << "Invalid stop decision. not stop at ahead of current "
                  "position. stop_s : "
               << stop_s << ", and current adc_s is; " << adc_front_s_;
        return Status(ErrorCode::PLANNING_ERROR, "invalid decision");
      }
      if (!stop_obstacle) {
        stop_obstacle = path_obstacle;
        stop_decision = decision;
        min_stop_s = stop_s;
      } else if (stop_s < min_stop_s) {
        stop_obstacle = path_obstacle;
        min_stop_s = stop_s;
        stop_decision = decision;
      }
    } else if (decision.has_overtake() || decision.has_yield()) {
      StGraphBoundary boundary;
      const auto ret =
          MapWithPredictionTrajectory(*path_obstacle, decision, &boundary);
      if (!ret.ok()) {
        AERROR << "Fail to map obstacle " << path_obstacle->Id()
               << " with decision: " << decision.DebugString();
        return Status(ErrorCode::PLANNING_ERROR,
                      "Fail to map overtake/yield decision");
      }
      AppendBoundary(boundary, st_graph_boundaries);
    } else {
      ADEBUG << "No mapping for decision: " << decision.DebugString();
    }
  }

  if (stop_obstacle) {
    StGraphBoundary stop_boundary;
    bool success =
        MapStopDecision(*stop_obstacle, stop_decision, &stop_boundary);
    if (!success) {
      std::string msg = "Fail to MapStopDecision.";
      AERROR << msg;
      return Status(ErrorCode::PLANNING_ERROR, msg);
    }
    AppendBoundary(stop_boundary, st_graph_boundaries);
  }
  for (const auto& st_graph_boundary : *st_graph_boundaries) {
    DCHECK_EQ(st_graph_boundary.points().size(), 4);
    DCHECK_NE(st_graph_boundary.id().length(), 0);
  }
  return Status::OK();
}

bool StBoundaryMapper::MapStopDecision(const PathObstacle& stop_obstacle,
                                       const ObjectDecisionType& stop_decision,
                                       StGraphBoundary* const boundary) const {
  CHECK_NOTNULL(boundary);
  DCHECK(stop_decision.has_stop()) << "Must have stop decision";

  PathPoint obstacle_point;
  if (stop_obstacle.perception_sl_boundary().start_s() >
      path_data_.frenet_frame_path().points().back().s()) {
    return true;
  }
  if (!path_data_.GetPathPointWithRefS(
          stop_obstacle.perception_sl_boundary().start_s(), &obstacle_point)) {
    AERROR << "Fail to get path point from reference s. The sl boundary of "
              "stop obstacle is: "
           << stop_obstacle.perception_sl_boundary().DebugString();
    return false;
  }

  const double st_stop_s =
      obstacle_point.s() + stop_decision.stop().distance_s() -
      vehicle_param_.front_edge_to_center() - FLAGS_decision_valid_stop_range;
  if (st_stop_s < 0.0) {
    AERROR << "obstacle st stop_s " << st_stop_s << " is less than 0.";
    return false;
  }

  const double s_min = st_stop_s;
  const double s_max =
      std::fmax(s_min, std::fmax(planning_distance_, reference_line_.length()));
  std::vector<STPoint> boundary_points;
  boundary_points.emplace_back(s_min, 0.0);
  boundary_points.emplace_back(s_min, planning_time_);
  boundary_points.emplace_back(s_max + st_boundary_config_.boundary_buffer(),
                               planning_time_);
  boundary_points.emplace_back(s_max, 0.0);

  *boundary = StGraphBoundary(boundary_points);
  boundary->SetBoundaryType(StGraphBoundary::BoundaryType::STOP);
  boundary->SetCharacteristicLength(st_boundary_config_.boundary_buffer());
  boundary->SetId(stop_obstacle.Id());
  return true;
}

Status StBoundaryMapper::MapWithoutDecision(
    const PathObstacle& path_obstacle, StGraphBoundary* const boundary) const {
  std::vector<STPoint> lower_points;
  std::vector<STPoint> upper_points;

  std::vector<STPoint> boundary_points;
  if (!GetOverlapBoundaryPoints(path_data_.discretized_path().path_points(),
                                *(path_obstacle.Obstacle()), &upper_points,
                                &lower_points)) {
    return Status::OK();
  }

  if (lower_points.size() > 0 && upper_points.size() > 0) {
    boundary_points.clear();
    boundary_points.emplace_back(
        lower_points.at(0).s() - st_boundary_config_.boundary_buffer(),
        lower_points.at(0).t() - st_boundary_config_.boundary_buffer());
    boundary_points.emplace_back(
        lower_points.back().s() - st_boundary_config_.boundary_buffer(),
        lower_points.back().t() + st_boundary_config_.boundary_buffer());
    boundary_points.emplace_back(
        upper_points.back().s() + st_boundary_config_.boundary_buffer(),
        upper_points.back().t() + st_boundary_config_.boundary_buffer());
    boundary_points.emplace_back(
        upper_points.at(0).s() + st_boundary_config_.boundary_buffer(),
        upper_points.at(0).t() - st_boundary_config_.boundary_buffer());
    if (lower_points.at(0).t() > lower_points.back().t() ||
        upper_points.at(0).t() > upper_points.back().t()) {
      AWARN << "lower/upper points are reversed.";
    }

    *boundary = StGraphBoundary(boundary_points);
    boundary->SetId(path_obstacle.Obstacle()->Id());
  }
  return Status::OK();
}

bool StBoundaryMapper::GetOverlapBoundaryPoints(
    const std::vector<PathPoint>& path_points, const Obstacle& obstacle,
    std::vector<STPoint>* upper_points,
    std::vector<STPoint>* lower_points) const {
  DCHECK_NOTNULL(upper_points);
  DCHECK_NOTNULL(lower_points);
  DCHECK(upper_points->empty());
  DCHECK(lower_points->empty());
  DCHECK_GT(path_points.size(), 0);

  if (path_points.size() == 0) {
    std::string msg = common::util::StrCat(
        "Too few points in path_data_.discretized_path(); size = ",
        path_points.size());
    AERROR << msg;
    return false;
  }

  const auto& trajectory = obstacle.Trajectory();
  if (trajectory.trajectory_point_size() == 0) {
    for (const auto& curr_point_on_path : path_points) {
      if (curr_point_on_path.s() > planning_distance_) {
        break;
      }
      const Box2d obs_box = obstacle.PerceptionBoundingBox();

      if (CheckOverlap(curr_point_on_path, obs_box,
                       st_boundary_config_.boundary_buffer())) {
        lower_points->emplace_back(curr_point_on_path.s(), 0.0);
        lower_points->emplace_back(curr_point_on_path.s(), planning_time_);
        upper_points->emplace_back(planning_distance_, 0.0);
        upper_points->emplace_back(planning_distance_, planning_time_);
        break;
      }
    }
  } else {
    for (int i = 0; i < trajectory.trajectory_point_size(); ++i) {
      const auto& trajectory_point = trajectory.trajectory_point(i);
      double trajectory_point_time = trajectory_point.relative_time();
      const Box2d obs_box = obstacle.GetBoundingBox(trajectory_point);
      int64_t low = 0;
      int64_t high = path_points.size() - 1;
      bool find_low = false;
      bool find_high = false;
      while (low < high) {
        if (find_low && find_high) {
          break;
        }
        if (!find_low) {
          if (!CheckOverlap(path_points[low], obs_box,
                            st_boundary_config_.boundary_buffer())) {
            ++low;
          } else {
            find_low = true;
          }
        }
        if (!find_high) {
          if (!CheckOverlap(path_points[high], obs_box,
                            st_boundary_config_.boundary_buffer())) {
            --high;
          } else {
            find_high = true;
          }
        }
      }
      if (find_high && find_low) {
        lower_points->emplace_back(
            path_points[low].s() - st_boundary_config_.point_extension(),
            trajectory_point_time);
        upper_points->emplace_back(
            path_points[high].s() + st_boundary_config_.point_extension(),
            trajectory_point_time);
      }
    }
  }
  DCHECK_EQ(lower_points->size(), upper_points->size());
  return (lower_points->size() > 0 && upper_points->size() > 0);
}

Status StBoundaryMapper::MapWithPredictionTrajectory(
    const PathObstacle& path_obstacle, const ObjectDecisionType& obj_decision,
    StGraphBoundary* const boundary) const {
  DCHECK_NOTNULL(boundary);
  DCHECK(obj_decision.has_follow() || obj_decision.has_yield() ||
         obj_decision.has_overtake())
      << "obj_decision must be follow or yield or overtake.\n"
      << obj_decision.DebugString();

  std::vector<STPoint> lower_points;
  std::vector<STPoint> upper_points;

  std::vector<STPoint> boundary_points;
  if (!GetOverlapBoundaryPoints(path_data_.discretized_path().path_points(),
                                *(path_obstacle.Obstacle()), &upper_points,
                                &lower_points)) {
    return Status(ErrorCode::PLANNING_ERROR, "PLANNING_ERROR");
  }
  if (lower_points.size() > 0 && upper_points.size() > 0) {
    boundary_points.clear();
    const double buffer = st_boundary_config_.boundary_buffer();

    boundary_points.emplace_back(
        std::fmax(0.0, lower_points.at(0).s() - buffer),
        lower_points.at(0).t());
    boundary_points.emplace_back(
        std::fmax(0.0, lower_points.back().s() - buffer),
        lower_points.back().t());
    boundary_points.emplace_back(upper_points.back().s() + buffer +
                                     st_boundary_config_.boundary_buffer(),
                                 upper_points.back().t());
    boundary_points.emplace_back(upper_points.at(0).s() + buffer,
                                 upper_points.at(0).t());
    if (lower_points.at(0).t() > lower_points.back().t() ||
        upper_points.at(0).t() > upper_points.back().t()) {
      AWARN << "lower/upper points are reversed.";
    }

    // change boundary according to obj_decision.
    StGraphBoundary::BoundaryType b_type =
        StGraphBoundary::BoundaryType::UNKNOWN;
    double characteristic_length = 0.0;
    if (obj_decision.has_follow()) {
      const auto& speed = path_obstacle.Obstacle()->Perception().velocity();
      const double scalar_speed = std::hypot(speed.x(), speed.y());
      const double minimal_follow_time =
          st_boundary_config_.minimal_follow_time();
      characteristic_length =
          std::fmax(scalar_speed * minimal_follow_time,
                    std::fabs(obj_decision.follow().distance_s())) +
          vehicle_param_.front_edge_to_center();

      boundary_points.at(0).set_s(boundary_points.at(0).s());
      boundary_points.at(1).set_s(boundary_points.at(1).s());
      boundary_points.at(3).set_t(-1.0);
      b_type = StGraphBoundary::BoundaryType::FOLLOW;
    } else if (obj_decision.has_yield()) {
      const double dis = std::fabs(obj_decision.yield().distance_s());
      characteristic_length = dis;
      if (boundary_points.at(0).s() - dis < 0.0) {
        boundary_points.at(0).set_s(
            std::fmax(boundary_points.at(0).s() - buffer, 0.0));
      } else {
        boundary_points.at(0).set_s(boundary_points.at(0).s() - dis);
      }

      if (boundary_points.at(1).s() - dis < 0.0) {
        boundary_points.at(1).set_s(
            std::fmax(boundary_points.at(1).s() - buffer, 0.0));
      } else {
        boundary_points.at(1).set_s(boundary_points.at(1).s() - dis);
      }
      b_type = StGraphBoundary::BoundaryType::YIELD;

    } else if (obj_decision.has_overtake()) {
      const double dis = std::fabs(obj_decision.overtake().distance_s());
      characteristic_length = dis;
      boundary_points.at(2).set_s(boundary_points.at(2).s() + dis);
      boundary_points.at(3).set_s(boundary_points.at(3).s() + dis);
    } else {
      DCHECK(false) << "Obj decision should be either yield or overtake: "
                    << obj_decision.DebugString();
    }
    *boundary = StGraphBoundary(boundary_points);
    boundary->SetBoundaryType(b_type);
    boundary->SetId(path_obstacle.Obstacle()->Id());
    boundary->SetCharacteristicLength(characteristic_length);
  }
  return Status::OK();
}

Status StBoundaryMapper::MapFollowDecision(
    const PathObstacle& path_obstacle, const ObjectDecisionType& obj_decision,
    StGraphBoundary* const boundary) const {
  DCHECK_NOTNULL(boundary);
  DCHECK(obj_decision.has_follow())
      << "Map obstacle without prediction trajectory is ONLY supported when "
         "the object decision is follow. The current object decision is: "
      << obj_decision.DebugString();

  const auto* obstacle = path_obstacle.Obstacle();
  SLPoint obstacle_sl_point;
  reference_line_.get_point_in_frenet_frame(
      Vec2d(obstacle->Perception().position().x(),
            obstacle->Perception().position().y()),
      &obstacle_sl_point);

  const auto& ref_point = reference_line_.get_reference_point(
      obstacle->Perception().position().x(),
      obstacle->Perception().position().y());

  const double speed_coeff =
      std::cos(obj_decision.follow().fence_heading() - ref_point.heading());
  if (speed_coeff < 0.0) {
    AERROR << "Obstacle is moving opposite to the reference line.";
    return common::Status(ErrorCode::PLANNING_ERROR,
                          "obstacle is moving opposite the reference line");
  }

  const auto& start_point = path_data_.discretized_path().StartPoint();
  SLPoint start_sl_point;
  if (!reference_line_.get_point_in_frenet_frame(
          Vec2d(start_point.x(), start_point.y()), &start_sl_point)) {
    std::string msg = "Fail to get s and l of start point.";
    AERROR << msg;
    return common::Status(ErrorCode::PLANNING_ERROR, msg);
  }

  const double distance_to_obstacle =
      obstacle_sl_point.s() -
      obstacle->Perception().length() / 2.0 *
          st_boundary_config_.expanding_coeff() -
      start_sl_point.s() - vehicle_param_.front_edge_to_center() -
      st_boundary_config_.follow_buffer();

  if (distance_to_obstacle > planning_distance_) {
    std::string msg = "obstacle is out of range.";
    ADEBUG << msg;
    return Status::OK();
  }

  const auto& velocity = obstacle->Perception().velocity();
  const double speed = std::hypot(velocity.x(), velocity.y());

  const double s_min_lower = distance_to_obstacle;
  const double s_min_upper =
      std::max(distance_to_obstacle + 1.0, planning_distance_);
  const double s_max_lower = s_min_lower + planning_time_ * speed;
  const double s_max_upper = std::max(s_max_lower, planning_distance_);

  std::vector<STPoint> boundary_points;
  boundary_points.emplace_back(s_min_lower, 0.0);
  boundary_points.emplace_back(s_max_lower, planning_time_);
  boundary_points.emplace_back(s_max_upper, planning_time_);
  boundary_points.emplace_back(s_min_upper, 0.0);

  *boundary = StGraphBoundary(boundary_points);

  const double characteristic_length =
      std::fabs(obj_decision.follow().distance_s()) +
      st_boundary_config_.follow_buffer();

  boundary->SetCharacteristicLength(characteristic_length);
  boundary->SetId(obstacle->Id());
  boundary->SetBoundaryType(StGraphBoundary::BoundaryType::FOLLOW);

  return Status::OK();
}

bool StBoundaryMapper::CheckOverlap(const PathPoint& path_point,
                                    const Box2d& obs_box,
                                    const double buffer) const {
  const double mid_to_rear_center =
      vehicle_param_.length() / 2.0 - vehicle_param_.front_edge_to_center();
  const double x =
      path_point.x() - mid_to_rear_center * std::cos(path_point.theta());
  const double y =
      path_point.y() - mid_to_rear_center * std::sin(path_point.theta());
  const Box2d adc_box =
      Box2d({x, y}, path_point.theta(), vehicle_param_.length() + 2 * buffer,
            vehicle_param_.width() + 2 * buffer);
  return obs_box.HasOverlap(adc_box);
}

Status StBoundaryMapper::GetSpeedLimits(
    SpeedLimit* const speed_limit_data) const {
  CHECK_NOTNULL(speed_limit_data);

  for (const auto& path_point : path_data_.discretized_path().path_points()) {
    if (Double::Compare(path_point.s(), reference_line_.length()) > 0) {
      std::string msg = common::util::StrCat(
          "path length [", path_data_.discretized_path().Length(),
          "] is LARGER than reference_line_ length [", reference_line_.length(),
          "]. Please debug before proceeding.");
      AWARN << msg;
      break;
    }

    double speed_limit_on_reference_line =
        reference_line_.GetSpeedLimitFromS(path_point.s());

    // speed limit from path curvature
    double speed_limit_on_path =
        std::sqrt(st_boundary_config_.centric_acceleration_limit() /
                  std::fmax(std::fabs(path_point.kappa()),
                            st_boundary_config_.minimal_kappa()));

    const double curr_speed_limit = std::fmax(
        st_boundary_config_.lowest_speed(),
        std::fmin(speed_limit_on_path, speed_limit_on_reference_line));

    speed_limit_data->AppendSpeedLimit(path_point.s(), curr_speed_limit);
  }
  return Status::OK();
}

void StBoundaryMapper::AppendBoundary(
    const StGraphBoundary& boundary,
    std::vector<StGraphBoundary>* st_graph_boundaries) const {
  if (Double::Compare(boundary.area(), 0.0) <= 0) {
    return;
  }
  st_graph_boundaries->push_back(std::move(boundary));
}

}  // namespace planning
}  // namespace apollo
