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

#ifndef MODULES_PERCEPTION_OBSTACLE_LIDAR_TRACKER_HM_TRACKER_BASE_MATCHER_H_
#define MODULES_PERCEPTION_OBSTACLE_LIDAR_TRACKER_HM_TRACKER_BASE_MATCHER_H_

#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "modules/common/macro.h"
#include "modules/perception/obstacle/lidar/tracker/hm_tracker/object_track.h"
#include "modules/perception/obstacle/lidar/tracker/hm_tracker/tracked_object.h"

namespace apollo {
namespace perception {

class BaseMatcher {
 public:
  typedef std::pair<int, int> TrackObjectPair;

  BaseMatcher() {}
  virtual ~BaseMatcher() {}

  // @brief match new detected objects to tracks build previously
  // @params[IN] objects: new detected objects
  // @params[IN] tracks: tracks build previously
  // @params[IN] tracks_predict: predicted states of tracks build previously
  // @params[IN] time_diff: time interval from last matching
  // @params[OUT] assignments: matched pair of objects & tracks
  // @params[OUT] unassigned_tracks: unmatched tracks
  // @params[OUT] unassigned_objects: unmatched objects
  // @return nothing
  virtual void Match(std::vector<TrackedObjectPtr>* objects,
                     const std::vector<ObjectTrackPtr>& tracks,
                     const std::vector<Eigen::VectorXf>& tracks_predict,
                     const double time_diff,
                     std::vector<TrackObjectPair>* assignments,
                     std::vector<int>* unassigned_tracks,
                     std::vector<int>* unassigned_objects) = 0;

  // @brief get name of matcher
  // @return name of matcher
  virtual std::string Name() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseMatcher);
};  // class BaseObjectTrackMatcher

}  // namespace perception
}  // namespace apollo

#endif  // MODULES_PERCEPTION_OBSTACLE_LIDAR_TRACKER_HM_TRACKER_BASE_MATCHER_H_
