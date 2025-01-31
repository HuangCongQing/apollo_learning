/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
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
#pragma once

#include <string>

#include "modules/perception/fusion/base/base_forward_declaration.h"
#include "modules/perception/fusion/base/scene.h"
#include "modules/perception/fusion/base/sensor_frame.h"
#include "modules/perception/lib/registerer/registerer.h"

namespace apollo {
namespace perception {
namespace fusion {
struct TrackerOptions {
  double match_distance = 0.0;
};
// 更新update track state with measurement(for foreground 只针对前景)
class BaseTracker {
 public:
  BaseTracker() {}
  virtual ~BaseTracker() {}
  BaseTracker(const BaseTracker&) = delete;
  BaseTracker& operator=(const BaseTracker&) = delete;

  virtual bool Init(TrackPtr track, SensorObjectPtr measurement) = 0;

  // @brief: update track state with measurement
  // @param [in]: measurement
  // @param [in]: target_timestamp
  // @param [in/out]: track
  virtual void UpdateWithMeasurement(const TrackerOptions& options,
                                     const SensorObjectPtr measurement,
                                     double target_timestamp) = 0;

  virtual void UpdateWithoutMeasurement(const TrackerOptions& options,
                                        const std::string& sensor_id,
                                        double measurement_timestamp,
                                        double target_timestamp) = 0;

  virtual std::string Name() const = 0;

 protected:
  TrackPtr track_ = nullptr; // 基于track_
};

}  // namespace fusion
}  // namespace perception
}  // namespace apollo
