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
 * @file reference_line.h
 **/

#ifndef MODULES_PLANNING_REFERENCE_LINE_REFERENCE_LINE_H_
#define MODULES_PLANNING_REFERENCE_LINE_REFERENCE_LINE_H_

#include <string>
#include <utility>
#include <vector>

#include "modules/common/proto/pnc_point.pb.h"

#include "modules/common/math/vec2d.h"
#include "modules/map/pnc_map/path.h"
#include "modules/planning/reference_line/reference_point.h"

namespace apollo {
namespace planning {

class ReferenceLine {
 public:
  ReferenceLine() = default;
  explicit ReferenceLine(const std::vector<ReferencePoint>& reference_points);
  explicit ReferenceLine(const hdmap::Path& hdmap_path);
  ReferenceLine(const std::vector<ReferencePoint>& reference_points,
                const std::vector<hdmap::LaneSegment>& lane_segments,
                const double max_approximation_error);

  const hdmap::Path& map_path() const;
  const std::vector<ReferencePoint>& reference_points() const;

  ReferencePoint get_reference_point(const double s) const;
  ReferencePoint get_reference_point(const double x, const double y) const;

  bool get_point_in_cartesian_frame(const common::SLPoint& sl_point,
                                    common::math::Vec2d* const xy_point) const;
  bool get_point_in_frenet_frame(const common::math::Vec2d& xy_point,
                                 common::SLPoint* const sl_point) const;

  bool get_lane_width(const double s, double* const left_width,
                      double* const right_width) const;
  bool is_on_road(const common::SLPoint& sl_point) const;

  double length() const { return map_path_.length(); }

  std::string DebugString() const;

  void get_s_range_from_box2d(const ::apollo::common::math::Box2d& box2d,
                              double* max_s, double* min_s) const;

  double GetSpeedLimitFromS(const double s) const;
  double GetSpeedLimitFromPoint(const common::math::Vec2d& point) const;

 private:
  /**
   * @brief Linearly interpolate p0 and p1 by s0 and s1.
   * The input has to satisfy condition: s0 <= s <= s1
   * p0 and p1 must have lane_waypoint.
   * Note: it requires p0 and p1 are on the same lane, adjacent lanes, or
   * parallel neighboring lanes. Otherwise the interpolated result may not
   * valid.
   * @param p0 the first anchor point for interpolation.
   * @param s0 the longitutial distance (s) of p0 on current reference line. s0
   * <= s && s0 <= s1
   * @param p1 the second anchor point for interpolation
   * @param s1 the longitutial distance (s) of p1 on current reference line. s1
   * @param s identifies the the middle point that is going to be interpolated.
   * s >= s0 && s <= s1
   * @return The interpolated ReferencePoint.
   */
  static ReferencePoint interpolate(const ReferencePoint& p0, const double s0,
                                    const ReferencePoint& p1, const double s1,
                                    const double s);

  static double find_min_distance_point(const ReferencePoint& p0,
                                        const double s0,
                                        const ReferencePoint& p1,
                                        const double s1, const double x,
                                        const double y);

 private:
  std::vector<ReferencePoint> reference_points_;
  hdmap::Path map_path_;
};

}  // namespace planning
}  // namespace apollo

#endif  // MODULES_PLANNING_REFERENCE_LINE_REFERENCE_LINE_H_
