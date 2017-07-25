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
#ifndef MODULES_MAP_MAP_LOADER_ADAPTER_XML_PARSER_UTIL_XML_PARSER_H
#define MODULES_MAP_MAP_LOADER_ADAPTER_XML_PARSER_UTIL_XML_PARSER_H

#include <vector>
#include <string>
#include "modules/map/hdmap/adapter/xml_parser/common_define.h"
#include "tinyxml2.h"
#include "modules/map/hdmap/adapter/xml_parser/status.h"

namespace apollo {
namespace hdmap {
namespace adapter {

class UtilXmlParser {
 public:
    static Status parse_geometry(const tinyxml2::XMLElement& xml_node,
                                PbCurveSegment* curve_segment);
    static Status parse_point_set(const tinyxml2::XMLElement& xml_node,
                                PbLineSegment* line_segment);
    static Status parse_outline(const tinyxml2::XMLElement& xml_node,
                                PbPolygon* polygon);
    static Status parse_point(const tinyxml2::XMLElement& xml_node,
                                PbPoint3D* pt);

    static std::vector<std::string> split(const std::string str,
                                        const std::string pattern);
    static std::string get_road_id(const std::string& lane_id);
    static std::string get_lane_id(const std::string& lane_id);
    static std::string get_traffic_light_sub_signal_id(const std::string& id);

    static std::string create_lane_id(const std::string& road_id,
                                    const std::string& section_id,
                                    const int lane_id);
    static void to_upper(std::string* s);

    static void utm_to_wgs84(const double x, const double y, const double z,
                        double* output_x, double* output_y, double* output_z);
    static void wgs84_to_utm(const double x, const double y, const double z,
                        double* output_x, double* output_y, double* output_z);

    static void get_map_boundary(double* x_min, double* x_max,
                                double* y_min, double* y_max);

    static double curve_length(PbCurve* curve);

    static tinyxml2::XMLError query_string_attribute(
                                        const tinyxml2::XMLElement& xml_node,
                                        const std::string& name,
                                        std::string* value);
    static tinyxml2::XMLElement* create_xml_element(const std::string& name);

 private:
    static double _x_min;
    static double _x_max;
    static double _y_min;
    static double _y_max;
};

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo

#endif  // MODULES_MAP_MAP_LOADER_ADAPTER_XML_PARSER_UTIL_XML_PARSER_H
