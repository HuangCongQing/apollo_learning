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
 * @file discretized_trajectory.cc
 **/

#include "modules/planning/common/trajectory/discretized_trajectory.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "modules/common/log.h"
#include "modules/planning/common/planning_util.h"

namespace apollo {
namespace planning {

using common::TrajectoryPoint;

DiscretizedTrajectory::DiscretizedTrajectory(
    std::vector<TrajectoryPoint> trajectory_points) {
  trajectory_points_ = std::move(trajectory_points);
}

double DiscretizedTrajectory::TimeLength() const {
  if (trajectory_points_.empty()) {
    return 0.0;
  }
  return trajectory_points_.back().relative_time() -
         trajectory_points_.front().relative_time();
}

TrajectoryPoint DiscretizedTrajectory::Evaluate(
    const double relative_time) const {
  CHECK(!trajectory_points_.empty());
  CHECK(trajectory_points_.front().relative_time() <= relative_time &&
        trajectory_points_.back().relative_time() <= relative_time)
      << "Invalid relative time input!";

  auto comp = [](const TrajectoryPoint& p, const double relative_time) {
    return p.relative_time() < relative_time;
  };

  auto it_lower =
      std::lower_bound(trajectory_points_.begin(), trajectory_points_.end(),
                       relative_time, comp);

  if (it_lower == trajectory_points_.begin()) {
    return trajectory_points_.front();
  }
  return util::interpolate(*(it_lower - 1), *it_lower, relative_time);
}

TrajectoryPoint DiscretizedTrajectory::EvaluateUsingLinearApproximation(
    const double relative_time) const {
  auto comp = [](const TrajectoryPoint& p, const double relative_time) {
    return p.relative_time() < relative_time;
  };

  auto it_lower =
      std::lower_bound(trajectory_points_.begin(), trajectory_points_.end(),
                       relative_time, comp);

  if (it_lower == trajectory_points_.begin()) {
    return trajectory_points_.front();
  }
  return util::interpolate_linear_approximation(*(it_lower - 1), *it_lower,
                                                relative_time);
}

std::uint32_t DiscretizedTrajectory::QueryNearestPoint(
    const double relative_time) const {
  auto func = [](const TrajectoryPoint& tp, const double relative_time) {
    return tp.relative_time() < relative_time;
  };
  auto it_lower =
      std::lower_bound(trajectory_points_.begin(), trajectory_points_.end(),
                       relative_time, func);
  return (std::uint32_t)(it_lower - trajectory_points_.begin());
}

std::uint32_t DiscretizedTrajectory::QueryNearestPoint(
    const common::math::Vec2d& position) const {
  double dist_min = std::numeric_limits<double>::max();
  std::uint32_t index_min = 0;
  for (std::uint32_t i = 0; i < trajectory_points_.size(); ++i) {
    const common::math::Vec2d coordinate(
        trajectory_points_[i].path_point().x(),
        trajectory_points_[i].path_point().y());
    common::math::Vec2d dist_vec = coordinate - position;
    double dist = dist_vec.InnerProd(dist_vec);
    if (dist < dist_min) {
      dist_min = dist;
      index_min = i;
    }
  }
  return index_min;
}

void DiscretizedTrajectory::AppendTrajectoryPoint(
    const TrajectoryPoint& trajectory_point) {
  if (!trajectory_points_.empty()) {
    CHECK_GT(trajectory_point.relative_time(),
             trajectory_points_.back().relative_time());
  }
  trajectory_points_.push_back(trajectory_point);
}

const TrajectoryPoint& DiscretizedTrajectory::TrajectoryPointAt(
    const std::uint32_t index) const {
  CHECK_LT(index, NumOfPoints());
  return trajectory_points_[index];
}

TrajectoryPoint DiscretizedTrajectory::StartPoint() const {
  CHECK(!trajectory_points_.empty());
  return trajectory_points_.front();
}

TrajectoryPoint DiscretizedTrajectory::EndPoint() const {
  CHECK(!trajectory_points_.empty());
  return trajectory_points_.back();
}

std::uint32_t DiscretizedTrajectory::NumOfPoints() const {
  return trajectory_points_.size();
}

const std::vector<TrajectoryPoint>&
DiscretizedTrajectory::trajectory_points() const {
  return trajectory_points_;
}

void DiscretizedTrajectory::set_trajectory_points(
    std::vector<TrajectoryPoint> points) {
  trajectory_points_ = std::move(points);
  CHECK(Valid() && "The input trajectory points have wrong relative time!");
}

bool DiscretizedTrajectory::Valid() const {
  std::size_t size = trajectory_points_.size();
  if (size == 0 || size == 1) {
    return true;
  }

  for (std::size_t i = 1; i < size; ++i) {
    if (!(trajectory_points_[i - 1].relative_time()
        < trajectory_points_[i].relative_time())) {
      return false;
    }
  }
  return true;
}

void DiscretizedTrajectory::Clear() { trajectory_points_.clear(); }

}  // namespace planning
}  // namespace apollo
