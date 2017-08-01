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

#include <algorithm>
#include <vector>
#include "modules/common/log.h"
#include "modules/perception/obstacle/common/geometry_util.h"
#include "modules/perception/obstacle/lidar/tracker/hm_tracker/track_object_distance.h"

namespace apollo {
namespace perception {

double TrackObjectDistance::s_weight_location_dist_ = 0.6;
double TrackObjectDistance::s_weight_direction_dist_ = 0.2;
double TrackObjectDistance::s_weight_bbox_size_dist_ = 0.1;
double TrackObjectDistance::s_weight_point_num_dist_ = 0.1;
double TrackObjectDistance::s_weight_histogram_dist_ = 0.5;

bool TrackObjectDistance::SetWeightLocationDist(float weight_location_dist) {
  // Set weight of location dist for all the track object distance objects
  if (weight_location_dist >= 0) {
    s_weight_location_dist_ = weight_location_dist;
    return true;
  } else {
    AERROR << "invalid weight_location_dist. TrackObjectDistance";
    return false;
  }
}

bool TrackObjectDistance::SetWeightDirectionDist(float weight_direction_dist) {
  // Set weight of direction dist for all the track object distance objects
  if (weight_direction_dist >= 0) {
    s_weight_direction_dist_ = weight_direction_dist;
    return true;
  } else {
    AERROR << "invalid weight_direction_dist. TrackObjectDistance";
    return false;
  }
}

bool TrackObjectDistance::SetWeightBboxSizeDist(float weight_bbox_size_dist) {
  // Set weight of bbox size dist for all the track object distance objects
  if (weight_bbox_size_dist >= 0) {
    s_weight_bbox_size_dist_ = weight_bbox_size_dist;
    return true;
  } else {
    AERROR << "invalid weight_bbox_size_dist. TrackedObjectDistance";
    return false;
  }
}

bool TrackObjectDistance::SetWeightPointNumDist(float weight_point_num_dist) {
  // Set weight of point num dist for all the track object distance objects
  if (weight_point_num_dist >= 0) {
    s_weight_point_num_dist_ = weight_point_num_dist;
    return true;
  } else {
    AERROR << "invalid weight_point_num_dist. TrackedObjectDistance";
    return false;
  }
}

bool TrackObjectDistance::SetWeightHistogramDist(float weight_histogram_dist) {
  // Set weight of histogram dist for all the track objects distance objects
  if (weight_histogram_dist >= 0) {
    s_weight_histogram_dist_ = weight_histogram_dist;
    return true;
  } else {
    AERROR << "invalid weight_histogram_dist. TrackedObjectDistance";
    return false;
  }
}

float TrackObjectDistance::ComputeDistance(const ObjectTrackPtr& track,
  const Eigen::VectorXf& track_predict,
  const TrackedObjectPtr& new_object,
  const double time_diff) {
  // Compute distance for given trakc & object
  float location_dist = ComputeLocationDistance(track, track_predict,
    new_object, time_diff);
  float direction_dist = ComputeDirectionDistance(track, track_predict,
    new_object, time_diff);
  float bbox_size_dist = ComputeBboxSizeDistance(track, track_predict,
    new_object, time_diff);
  float point_num_dist = ComputePointNumDistance(track, track_predict,
    new_object, time_diff);
  float histogram_dist = ComputeHistogramDistance(track, track_predict,
    new_object, time_diff);

  float result_dist = s_weight_location_dist_ * location_dist +
    s_weight_direction_dist_ * direction_dist +
    s_weight_bbox_size_dist_ * bbox_size_dist +
    s_weight_point_num_dist_ * point_num_dist +
    s_weight_histogram_dist_ * histogram_dist;

  return result_dist;
}

float TrackObjectDistance::ComputeLocationDistance(const ObjectTrackPtr& track,
  const Eigen::VectorXf& track_predict,
  const TrackedObjectPtr& new_object,
  const double time_diff) {
  // Compute locatin distance for given track & object
  // range from 0 to positive infinity
  const TrackedObjectPtr& last_object = track->current_object_;

  Eigen::Vector3f measured_anchor_point = new_object->anchor_point;
  Eigen::Vector3f predicted_anchor_point = track_predict.head(3);
  Eigen::Vector3f measure_predict_diff = measured_anchor_point -
    predicted_anchor_point;

  float location_dist = sqrt((measure_predict_diff.head(2).cwiseProduct(
    measure_predict_diff.head(2))).sum());

  /* NEED TO NOTICE: All the states would be collected mainly based on states of tracked objects.
   * Thus, update tracked object when you update the state of track !!!!! */
  Eigen::Vector2f ref_dir = last_object->velocity.head(2);
  float speed = ref_dir.norm();
  ref_dir /= speed;

  /* Let location distance generated from a normal distribution with symmetric variance. 
   * Modify its variance when speed greater than a threshold. Penalize variance other than 
   * motion direction. */
  if (speed > 2) {
    Eigen::Vector2f ref_o_dir = Eigen::Vector2f(ref_dir[1], -ref_dir[0]);
    float dx = ref_dir(0) * measure_predict_diff(0) + ref_dir(1) *
      measure_predict_diff(1);
    float dy = ref_o_dir(0) * measure_predict_diff(0) + ref_o_dir(1) *
      measure_predict_diff(1);
    location_dist = sqrt(dx * dx * 0.25 + dy * dy * 4);
  }

  return location_dist;
}

float TrackObjectDistance::ComputeDirectionDistance(const ObjectTrackPtr& track,
  const Eigen::VectorXf& track_predict,
  const TrackedObjectPtr& new_object,
  const double time_diff) {
  // Compute direction distance for given track & object
  // range from 0 to 2
  const TrackedObjectPtr& last_object = track->current_object_;

  Eigen::Vector3f old_anchor_point = last_object->anchor_point;
  Eigen::Vector3f new_anchor_point = new_object->anchor_point;
  Eigen::Vector3f anchor_point_shift_dir = new_anchor_point - old_anchor_point;
  anchor_point_shift_dir[2] = 0.0;

  Eigen::Vector3f track_motion_dir = track_predict.head(6).tail(3);
  track_motion_dir[2] = 0.0;

  double cos_theta = 0.994;  // average cos
  if (!track_motion_dir.head(2).isZero() &&
    !anchor_point_shift_dir.head(2).isZero()) {
    cos_theta = vector_cos_theta_2d_xy(track_motion_dir,
      anchor_point_shift_dir);
  }
  float direction_dist = -cos_theta + 1.0;

  return direction_dist;
}

float TrackObjectDistance::ComputeBboxSizeDistance(const ObjectTrackPtr& track,
  const Eigen::VectorXf& track_predict,
  const TrackedObjectPtr& new_object,
  const double time_diff) {
  // Compute bbox size distance for given track & object
  // range from 0 to 1
  const TrackedObjectPtr& last_object = track->current_object_;

  Eigen::Vector3f old_bbox_dir = last_object->direction;
  Eigen::Vector3f new_bbox_dir = new_object->direction;
  Eigen::Vector3f old_bbox_size = last_object->size;
  Eigen::Vector3f new_bbox_size = new_object->size;

  float size_dist = 0.0;
  double dot_val_00 = fabs(old_bbox_dir(0) * new_bbox_dir(0) +
    old_bbox_dir(1) * new_bbox_dir(1));
  double dot_val_01 = fabs(old_bbox_dir(0) * new_bbox_dir(1) -
    old_bbox_dir(1) * new_bbox_dir(0));

  if (dot_val_00 > dot_val_01) {
    float temp_val_0 = fabs(old_bbox_size(0) - new_bbox_size(0)) /
      std::max(old_bbox_size(0), new_bbox_size(0));
    float temp_val_1 = fabs(old_bbox_size(1) - new_bbox_size(1)) /
      std::max(old_bbox_size(1), new_bbox_size(1));
    size_dist = std::min(temp_val_0, temp_val_1);
  } else {
    float temp_val_0 = fabs(old_bbox_size(0) - new_bbox_size(1)) /
      std::max(old_bbox_size(0), new_bbox_size(1));
    float temp_val_1 = fabs(old_bbox_size(1) - new_bbox_size(0)) /
      std::max(old_bbox_size(1), new_bbox_size(0));
    size_dist = std::min(temp_val_0, temp_val_1);
  }

  return size_dist;
}

float TrackObjectDistance::ComputePointNumDistance(const ObjectTrackPtr& track,
  const Eigen::VectorXf& track_predict,
  const TrackedObjectPtr& new_object,
  const double time_diff) {
  // Compute point num distance for given track & object
  // range from 0 and 1
  const TrackedObjectPtr& last_object = track->current_object_;

  int old_point_number = last_object->object_ptr->cloud->size();
  int new_point_number = new_object->object_ptr->cloud->size();

  float point_num_dist = fabs(old_point_number - new_point_number) * 1.0f /
    std::max(old_point_number, new_point_number);

  return point_num_dist;
}

float TrackObjectDistance::ComputeHistogramDistance(const ObjectTrackPtr& track,
  const Eigen::VectorXf& track_predict,
  const TrackedObjectPtr& new_object,
  const double time_diff) {
  // Compute histogram distance for given track & object
  // range from 0 to 3
  const TrackedObjectPtr& last_object = track->current_object_;

  std::vector<float>& old_object_shape_features =
    last_object->object_ptr->shape_features;
  std::vector<float>& new_object_shape_features =
    new_object->object_ptr->shape_features;

  if (old_object_shape_features.size() != new_object_shape_features.size()) {
    AERROR << "sizes of compared features not matched. TrackObjectDistance";
    return 100;
  }

  float histogram_dist = 0.0;
  for (int i = 0; i < old_object_shape_features.size(); ++i) {
    histogram_dist += std::fabs(old_object_shape_features[i] -
      new_object_shape_features[i]);
  }

  return histogram_dist;
}

}  // namespace perception
}  // namespace apollo
