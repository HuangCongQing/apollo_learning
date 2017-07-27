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

#include "modules/planning/planner/rtk/rtk_replay_planner.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "modules/planning/common/planning_gflags.h"

using apollo::common::TrajectoryPoint;

namespace apollo {
namespace planning {

class RTKReplayPlannerTest : public ::testing::Test {};

TEST_F(RTKReplayPlannerTest, ComputeTrajectory) {
  FLAGS_rtk_trajectory_filename = "modules/planning/testdata/garage.csv";
  RTKReplayPlanner planner;

  TrajectoryPoint start_point;
  start_point.mutable_path_point()->set_x(586385.782842);
  start_point.mutable_path_point()->set_y(4140674.76063);

  std::vector<TrajectoryPoint> trajectory;
  auto status = planner.MakePlan(start_point, &trajectory);

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(!trajectory.empty());
  EXPECT_EQ(trajectory.size(), (std::uint32_t)FLAGS_rtk_trajectory_forward);

  auto first_point = trajectory.front();
  EXPECT_DOUBLE_EQ(first_point.path_point().x(), 586385.782841);
  EXPECT_DOUBLE_EQ(first_point.path_point().y(), 4140674.76065);

  auto last_point = trajectory.back();
  EXPECT_DOUBLE_EQ(last_point.path_point().x(), 586355.063786);
  EXPECT_DOUBLE_EQ(last_point.path_point().y(), 4140681.98605);
}

TEST_F(RTKReplayPlannerTest, ErrorTest) {
  FLAGS_rtk_trajectory_filename =
      "modules/planning/testdata/garage_no_file.csv";
  RTKReplayPlanner planner;
  FLAGS_rtk_trajectory_filename = "modules/planning/testdata/garage_error.csv";
  RTKReplayPlanner planner_with_error_csv;
  TrajectoryPoint start_point;
  start_point.mutable_path_point()->set_x(586385.782842);
  start_point.mutable_path_point()->set_y(4140674.76063);
  std::vector<TrajectoryPoint> trajectory;
  EXPECT_TRUE(
      !(planner_with_error_csv.MakePlan(start_point, &trajectory)).ok());
}

}  // namespace planning
}  // namespace apollo
