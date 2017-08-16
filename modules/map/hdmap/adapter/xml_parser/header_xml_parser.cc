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
#include "modules/map/hdmap/adapter/xml_parser/header_xml_parser.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <string>
#include "glog/logging.h"
#include "modules/map/hdmap/adapter/coordinate_convert_tool.h"
#include "modules/map/hdmap/adapter/xml_parser/util_xml_parser.h"

namespace apollo {
namespace hdmap {
namespace adapter {

Status HeaderXmlParser::parse(const tinyxml2::XMLElement& xml_node,
                        PbHeader* header) {
  auto header_node = xml_node.FirstChildElement("header");
  if (!header_node) {
      std::string err_msg = "error xml data missing header";
      return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
  }
  int rev_major = 0;
  int rev_minor = 0;
  std::string database_name;
  float version = 0.0;
  std::string date;
  double north = 0.0;
  double south = 0.0;
  double west = 0.0;
  double east = 0.0;
  std::string vendor;
  int checker = header_node->QueryIntAttribute("revMajor", &rev_major);
  checker += header_node->QueryIntAttribute("revMinor", &rev_minor);
  checker += UtilXmlParser::query_string_attribute(*header_node, "name",
                                                  &database_name);
  checker += header_node->QueryFloatAttribute("version", &version);
  checker += UtilXmlParser::query_string_attribute(*header_node, "date",
                                                  &date);
  checker += header_node->QueryDoubleAttribute("north", &north);
  checker += header_node->QueryDoubleAttribute("south", &south);
  checker += header_node->QueryDoubleAttribute("east", &east);
  checker += header_node->QueryDoubleAttribute("west", &west);
  checker += UtilXmlParser::query_string_attribute(*header_node, "vendor",
                                                    &vendor);

  if (checker != tinyxml2::XML_SUCCESS) {
    std::string err_msg = "Error parsing header attributes";
    return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
  }

  auto geo_reference_node = header_node->FirstChildElement("geoReference");
  if (!geo_reference_node) {
    std::string err_msg = "Error parsing header geoReoference attributes";
    return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
  }
  auto geo_text = geo_reference_node->FirstChild()->ToText();
  if (!geo_text) {
    std::string err_msg = "Error parsing header geoReoference text";
    return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
  }
  
  // coordinate frame
  std::string from_coordinate = "+proj=longlat \
                    +ellps=WGS84 +datum=WGS84 +no_defs";
  std::string to_coordinate = "+proj=utm +zone=10 +ellps=WGS84 \
                    +datum=WGS84 +units=m +no_defs";
  CoordinateConvertTool::get_instance()->set_convert_param(from_coordinate,
                                                            to_coordinate);

  header->set_version(std::to_string(version));
  header->set_date(date);
  header->set_district(database_name);

  return Status::OK();
}

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
