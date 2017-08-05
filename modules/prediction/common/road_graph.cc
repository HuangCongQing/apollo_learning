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

#include "modules/prediction/common/road_graph.h"

#include <algorithm>
#include <utility>

#include "modules/common/util/string_util.h"
#include "modules/prediction/common/prediction_map.h"

namespace apollo {
namespace prediction {

using apollo::hdmap::LaneInfo;
using apollo::hdmap::Id;
using apollo::common::ErrorCode;
using apollo::common::Status;

RoadGraph::RoadGraph(double start_s, double length,
                     std::shared_ptr<const LaneInfo> lane_info_ptr)
    : start_s_(start_s), length_(length), lane_info_ptr_(lane_info_ptr) {
}

Status RoadGraph::BuildLaneGraph(LaneGraph* lane_graph_ptr) {
  if (length_ < 0.0 || lane_info_ptr_ == nullptr) {
    const auto error_msg = apollo::common::util::StrCat(
        "Invalid road graph settings. Road graph length = ", length_);
    AERROR << error_msg;
    return Status(ErrorCode::PREDICTION_ERROR, error_msg);
  }

  if (lane_graph_ptr == nullptr) {
    const auto error_msg = "Invalid input lane graph.";
    AERROR << error_msg;
    return Status(ErrorCode::PREDICTION_ERROR, error_msg);
  }

  std::vector<LaneSegment> lane_segments;
  double accumulated_s = 0.0;
  ComputeLaneSequence(accumulated_s, start_s_, lane_info_ptr_,
                      &lane_segments, lane_graph_ptr);

  return Status::OK();
}

void RoadGraph::ComputeLaneSequence(
    double accumulated_s, double start_s,
    std::shared_ptr<const LaneInfo> lane_info_ptr,
    std::vector<LaneSegment>* lane_segments,
    LaneGraph* lane_graph_ptr) const {
  if (lane_info_ptr == nullptr) {
    AERROR << "Invalid lane.";
    return;
  }
  PredictionMap* map = PredictionMap::instance();

  LaneSegment lane_segment;
  lane_segment.set_lane_id(lane_info_ptr->id().id());
  lane_segment.set_start_s(start_s);
  lane_segment.set_lane_turn_type(map->LaneTurnType(lane_info_ptr->id()));
  if (accumulated_s + lane_info_ptr->total_length() - start_s >= length_) {
    lane_segment.set_end_s(length_ - accumulated_s + start_s);
  } else {
    lane_segment.set_end_s(lane_info_ptr->total_length());
  }

  lane_segments->push_back(std::move(lane_segment));

  if (accumulated_s + lane_info_ptr->total_length() - start_s >= length_ ||
    lane_info_ptr->lane().successor_id_size() == 0) {
    LaneSequence* sequence = lane_graph_ptr->add_lane_sequence();
    *sequence->mutable_lane_segment() = {lane_segments->begin(),
                                         lane_segments->end()};
    sequence->set_label(0);
  } else {
    const double successor_accumulated_s =
        accumulated_s + lane_info_ptr->total_length() - start_s;
    for (const auto& successor_lane_id : lane_info_ptr->lane().successor_id()) {
      std::shared_ptr<const LaneInfo> successor_lane =
          map->LaneById(successor_lane_id);
      ComputeLaneSequence(successor_accumulated_s, 0.0, successor_lane,
                          lane_segments, lane_graph_ptr);
    }
  }
  lane_segments->pop_back();
}

}  // namespace prediction
}  // namespace apollo
