/* Copyright 2017 The Apollo Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
=========================================================================*/
#include "modules/map/hdmap/adapter/coordinate_convert_tool.h"
#include <math.h>
#include "glog/logging.h"

namespace apollo {
namespace hdmap {
namespace adapter {

CoordinateConvertTool::CoordinateConvertTool()
  : _pj_from_(NULL), _pj_to_(NULL) {}

CoordinateConvertTool::~CoordinateConvertTool() {
  if (_pj_from_) {
    pj_free(_pj_from_);
    _pj_from_ = NULL;
  }

  if (_pj_to_) {
    pj_free(_pj_to_);
    _pj_to_ = NULL;
  }
}

CoordinateConvertTool* CoordinateConvertTool::get_instance() {
  static CoordinateConvertTool instance;
  return &instance;
}

Status CoordinateConvertTool::set_convert_param(const std::string &source_param,
                                             const std::string &dst_param) {
  source_convert_param_ = source_param;
  dst_convert_param_ = dst_param;
  if (_pj_from_) {
      pj_free(_pj_from_);
      _pj_from_ = NULL;
  }

  if (_pj_to_) {
    pj_free(_pj_to_);
    _pj_to_ = NULL;
  }

  if (!(_pj_from_ = pj_init_plus(source_convert_param_.c_str()))) {
    std::string err_msg = "Fail to pj_init_plus " + source_convert_param_;
    return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
  }

  if (!(_pj_to_ = pj_init_plus(dst_convert_param_.c_str()))) {
    std::string err_msg = "Fail to pj_init_plus " + dst_convert_param_;
    pj_free(_pj_from_);
    _pj_from_ = NULL;
    return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
  }

  return Status::OK();
}

Status CoordinateConvertTool::coordiate_convert(const double longitude,
                                             const double latitude,
                                             const double height_ellipsoid,
                                             double* ltm_x, double* ltm_y,
                                             double* ltm_z) {
  CHECK_NOTNULL(ltm_x);
  CHECK_NOTNULL(ltm_y);
  CHECK_NOTNULL(ltm_z);
  if (!_pj_from_ || !_pj_to_) {
      std::string err_msg = "no transform param";
      return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
  }

  double gps_longitude = longitude;
  double gps_latitude = latitude;
  double gps_alt = height_ellipsoid;

  if (pj_is_latlong(_pj_from_)) {
    gps_longitude *= DEG_TO_RAD;
    gps_latitude *= DEG_TO_RAD;
    gps_alt = height_ellipsoid;
  }

  if (0 != pj_transform(_pj_from_, _pj_to_, 1, 1, &gps_longitude,
                          &gps_latitude, &gps_alt)) {
    std::string err_msg = "fail to transform coordinate";
    return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
  }

  if (pj_is_latlong(_pj_to_)) {
    gps_longitude *= RAD_TO_DEG;
    gps_latitude *= RAD_TO_DEG;
  }

  *ltm_x = gps_longitude;
  *ltm_y = gps_latitude;
  *ltm_z = gps_alt;

  return Status::OK();
}

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
