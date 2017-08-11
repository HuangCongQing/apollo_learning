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

#include <string>

#include "gtest/gtest.h"

#include "modules/common/configs/config_gflags.h"
#include "modules/planning/common/planning_gflags.h"
#include "modules/planning/integration_tests/planning_test_base.h"
#include "modules/planning/planning.h"

namespace apollo {
namespace planning {

using common::adapter::AdapterManager;

DECLARE_string(test_routing_response_file);
DECLARE_string(test_localization_file);
DECLARE_string(test_chassis_file);

/**
 * @class GarageTest
 * @brief This is an integration test that uses the garage map.
 */

class GarageTest : public PlanningTestBase {
 public:
  virtual void SetUp() {
    FLAGS_map_file_path = "modules/planning/testdata/base_map.txt";
  }
};

TEST_F(GarageTest, Stop) {
  FLAGS_test_prediction_file =
      "modules/planning/testdata/garage_test/"
      "stop_obstacle_prediction.pb.txt";
  FLAGS_test_localization_file =
      "modules/planning/testdata/garage_test/"
      "stop_obstacle_localization.pb.txt";
  FLAGS_test_chassis_file =
      "modules/planning/testdata/garage_test/"
      "stop_obstacle_chassis.pb.txt";
  // EXPECT_DEATH(PlanningTestBase::SetUp(), ".*SetUpAdapters.*");
  PlanningTestBase::SetUp();
  RunPlanning();
  ASSERT_TRUE(adc_trajectory_ != nullptr);
}

}  // namespace planning
}  // namespace apollo
