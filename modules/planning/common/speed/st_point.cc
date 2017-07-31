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
 * @file st_point.cpp
 **/

#include "modules/planning/common/speed/st_point.h"

#include <iomanip>

#include "modules/common/util/string_util.h"

namespace apollo {
namespace planning {

STPoint::STPoint(const double s, const double t) : Vec2d(t, s) {}

double STPoint::s() const { return y_; }

double STPoint::t() const { return x_; }

void STPoint::set_s(const double s) { return set_y(s); }

void STPoint::set_t(const double t) { return set_x(t); }

std::string STPoint::DebugString() const {
  return apollo::common::util::StrCat("{ \"s\" : ", std::setprecision(6), s(),
                                      ", "
                                      "\"t\" : ",
                                      std::setprecision(6), t(), " }");
}

}  // namespace planning
}  // namespace apollo
