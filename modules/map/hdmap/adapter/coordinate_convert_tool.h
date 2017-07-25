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
#ifndef MODULES_MAP_MAP_LOADER_ADAPTER_COORDINATE_CONVERT_TOOL_H
#define MODULES_MAP_MAP_LOADER_ADAPTER_COORDINATE_CONVERT_TOOL_H
#include <proj_api.h>
#include <string>
#include "modules/map/hdmap/adapter/xml_parser/status.h"

namespace apollo {
namespace hdmap  {
namespace adapter {

class CoordinateConvertTool {
 public:
  CoordinateConvertTool();
  ~CoordinateConvertTool();
 public:
  static CoordinateConvertTool* get_instance();

 public:
  Status set_convert_param(const std::string &source_param,
                        const std::string &dst_param);
  Status coordiate_convert(const double longitude, const double latitude,
                          const double height_ellipsoid, double* ltm_x,
                          double* ltm_y, double* ltm_z);

 private:
  std::string source_convert_param_;
  std::string dst_convert_param_;

  projPJ _pj_from_;
  projPJ _pj_to_;
};

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo

#endif  // MODULES_MAP_MAP_LOADER_ADAPTER_COORDINATE_CONVERT_TOOL_H
