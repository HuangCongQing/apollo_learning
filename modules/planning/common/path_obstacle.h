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

#ifndef MODULES_PLANNING_COMMON_PATH_OBSTACLE_H_
#define MODULES_PLANNING_COMMON_PATH_OBSTACLE_H_

#include <list>
#include <string>
#include <vector>
#include <unordered_map>

#include "gtest/gtest_prod.h"

#include "modules/perception/proto/perception_obstacle.pb.h"
#include "modules/planning/proto/decision.pb.h"
#include "modules/planning/proto/sl_boundary.pb.h"
#include "modules/prediction/proto/prediction_obstacle.pb.h"

#include "modules/common/math/box2d.h"
#include "modules/common/math/vec2d.h"
#include "modules/planning/common/indexed_list.h"
#include "modules/planning/common/obstacle.h"
#include "modules/planning/reference_line/reference_line.h"

namespace apollo {
namespace planning {

/**
 * @class PathObstacle
 * @brief This is the class that associates an Obstacle with its path
 * properties. An obstacle's path properties relative to a path.
 * The `s` and `l` values are examples of path properties.
 * The decision of an obstacle is also associated with a path.
 *
 * The decisions have two categories: lateral decision and longitudinal
 * decision.
 * Lateral decision includes: nudge, ignore.
 * Lateral decision saftey priority: nudge > ignore.
 * Longitudinal decision includes: stop, yield, follow, overtake, ignore.
 * Decision saftey priorities order: stop > yield >= follow > overtake > ignore
 *
 * Ignore decision belongs to both lateral decision and longitudinal decision,
 * and it has the lowest priority.
 */
class PathObstacle {
 public:
  PathObstacle();

  explicit PathObstacle(const planning::Obstacle* obstacle);

  bool Init(const ReferenceLine* reference_line);

  const std::string& Id() const;

  const planning::Obstacle* Obstacle() const;

  /**
   * @class Add decision to this obstacle. This function will
   * also calculate the merged decision. i.e., if there are multiple lateral
   * decisions or longitudinal decisions, this function will merge them into
   * one. The rule of merging decisions are as follows
   * * Priorities of different decisions: stop > yield > follow > overtake >
   * ignore
   * * Priorities of the same decisions: decision1.distance_s <
   * decision2.distance_s
   * @param decider_tag identifies which component added this decision
   * @param the decision to be added to this path obstacle.
   **/
  void AddDecision(const std::string& decider_tag,
                   const ObjectDecisionType& decision);

  bool HasLateralDecision() const;
  bool HasLongitudinalDecision() const;
  /**
   * return the merged lateral decision
   * Lateral decision is one of {Nudge, Ignore}
   **/
  const ObjectDecisionType& LateralDecision() const;

  /**
   * @brief return the merged longitudinal decision
   * Longitudinal decision is one of {Stop, Yield, Follow, Overtake, Ignore}
   **/
  const ObjectDecisionType& LongitudinalDecision() const;

  const std::string DebugString() const;

  const SLBoundary& perception_sl_boundary() const;

 private:
  bool InitPerceptionSLBoundary(const ReferenceLine* reference_line);

  /**
   * @brief check if a ObjectDecisionType is a lateral decision.
   **/
  FRIEND_TEST(IsLateralDecision, AllDecisions);
  static bool IsLateralDecision(const ObjectDecisionType& decision);

  /**
   * @brief check if a ObjectDecisionType is longitudinal decision.
   **/
  FRIEND_TEST(IsLongitudinalDecision, AllDecisions);
  static bool IsLongitudinalDecision(const ObjectDecisionType& decision);

  FRIEND_TEST(MergeLongitudinalDecision, AllDecisions);
  static ObjectDecisionType MergeLongitudinalDecision(
      const ObjectDecisionType& lhs, const ObjectDecisionType& rhs);
  FRIEND_TEST(MergeLateralDecision, AllDecisions);
  static ObjectDecisionType MergeLateralDecision(const ObjectDecisionType& lhs,
                                                 const ObjectDecisionType& rhs);

  std::string id_;
  const planning::Obstacle* obstacle_ = nullptr;
  std::vector<ObjectDecisionType> decisions_;
  std::vector<std::string> decider_tags_;
  SLBoundary perception_sl_boundary_;

  // TODO(zhangliangliang): add st_boundary_ here.
  // StGraphBoundary st_boundary_;

  ObjectDecisionType lateral_decision_;
  bool has_lateral_decision_ = false;
  ObjectDecisionType longitudinal_decision_;
  bool has_longitudinal_decision_ = false;

  struct ObjectTagCaseHash {
    std::size_t operator()(
        const planning::ObjectDecisionType::ObjectTagCase tag) const {
      return static_cast<std::size_t>(tag);
    }
  };

  static const std::unordered_map<ObjectDecisionType::ObjectTagCase, int,
                                  ObjectTagCaseHash>
      s_lateral_decision_safety_sorter_;
  static const std::unordered_map<ObjectDecisionType::ObjectTagCase, int,
                                  ObjectTagCaseHash>
      s_longitudinal_decision_safety_sorter_;
};

}  // namespace planning
}  // namespace apollo

#endif  // MODULES_PLANNING_COMMON_PATH_OBSTACLE_H_
