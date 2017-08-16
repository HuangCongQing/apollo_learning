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
#include "modules/map/hdmap/adapter/xml_parser/objects_xml_parser.h"
#include <string>
#include <vector>
#include "modules/map/hdmap/adapter/xml_parser/util_xml_parser.h"
#include "glog/logging.h"

namespace apollo {
namespace hdmap {
namespace adapter {

Status ObjectsXmlParser::parse_crosswalks(const tinyxml2::XMLElement& xml_node,
                                std::vector<PbCrosswalk>* crosswalks) {
  CHECK_NOTNULL(crosswalks);
  const tinyxml2::XMLElement* sub_node = xml_node.FirstChildElement("object");
  while (sub_node) {
    std::string object_type;
    std::string object_id;
    int checker = UtilXmlParser::query_string_attribute(*sub_node,
                                                    "type", &object_type);
    checker += UtilXmlParser::query_string_attribute(*sub_node,
                                                    "id", &object_id);
    if (checker != tinyxml2::XML_SUCCESS) {
        std::string err_msg = "Error parse object type.";
        return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
    }

    if (object_type == "crosswalk") {
      PbCrosswalk crosswalk;
      crosswalk.mutable_id()->set_id(object_id);
      PbPolygon* polygon = crosswalk.mutable_polygon();
      const tinyxml2::XMLElement* outline_node =
                                      sub_node->FirstChildElement("outline");
      RETURN_IF_ERROR(UtilXmlParser::parse_outline(*outline_node, polygon));
      crosswalks->emplace_back(crosswalk);
    }
    sub_node = sub_node->NextSiblingElement("object");
  }
  return Status::OK();
}

Status ObjectsXmlParser::parse_clear_areas(const tinyxml2::XMLElement& xml_node,
                                    std::vector<PbClearArea>* clear_areas) {
  CHECK_NOTNULL(clear_areas);
  const tinyxml2::XMLElement* sub_node = xml_node.FirstChildElement("object");
  while (sub_node) {
    std::string object_type;
    std::string object_id;
    int checker = UtilXmlParser::query_string_attribute(*sub_node,
                                                    "id", &object_id);
    checker += UtilXmlParser::query_string_attribute(*sub_node,
                                                    "type", &object_type);
    if (checker != tinyxml2::XML_SUCCESS) {
      std::string err_msg = "Error parse object type.";
      return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
    }

    if (object_type == "clearArea") {
      PbClearArea clear_area;
      clear_area.mutable_id()->set_id(object_id);
      PbPolygon* polygon = clear_area.mutable_polygon();
      const tinyxml2::XMLElement* outline_node =
                                    sub_node->FirstChildElement("outline");
      RETURN_IF_ERROR(UtilXmlParser::parse_outline(*outline_node, polygon));
      clear_areas->emplace_back(clear_area);
    }
    sub_node = sub_node->NextSiblingElement("object");
  }

  return Status::OK();
}

Status  ObjectsXmlParser::parse_speed_bumps(
                                    const tinyxml2::XMLElement& xml_node,
                                    std::vector<PbSpeedBump>* speed_bumps) {
  CHECK_NOTNULL(speed_bumps);
  const tinyxml2::XMLElement* object_node
                                        = xml_node.FirstChildElement("object");
  while (object_node) {
    std::string object_type;
    std::string object_id;
    int checker = UtilXmlParser::query_string_attribute(*object_node,
                                                    "id", &object_id);
    checker += UtilXmlParser::query_string_attribute(*object_node,
                                                    "type", &object_type);
    if (checker != tinyxml2::XML_SUCCESS) {
      std::string err_msg = "Error parse object type.";
      return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
    }

    if (object_type == "speedBump") {
      PbSpeedBump speed_bump;
      const tinyxml2::XMLElement* sub_node =
                                object_node->FirstChildElement("geometry");
      speed_bump.mutable_id()->set_id(object_id);
      while (sub_node) {
        PbCurve* curve = speed_bump.add_position();
        PbCurveSegment* curve_segment = curve->add_segment();
        RETURN_IF_ERROR(UtilXmlParser::parse_geometry(*sub_node,
                                                    curve_segment));
        sub_node = sub_node->NextSiblingElement("geometry");
      }
      if (speed_bump.position_size() <= 0) {
        std::string err_msg = "Error speed bump miss stop line.";
        return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
      }
      speed_bumps->emplace_back(speed_bump);
    }
    object_node = object_node->NextSiblingElement("object");
  }
  return Status::OK();
}

Status ObjectsXmlParser::parse_stop_lines(const tinyxml2::XMLElement& xml_node,
                        std::vector<StopLineInternal>* stop_lines) {
  CHECK_NOTNULL(stop_lines);
  const tinyxml2::XMLElement* object_node
                                      = xml_node.FirstChildElement("object");
  while (object_node) {
    std::string object_type;
    std::string object_id;
    int checker = UtilXmlParser::query_string_attribute(*object_node,
                                                      "id", &object_id);
    checker += UtilXmlParser::query_string_attribute(*object_node,
                                                      "type", &object_type);
    if (checker != tinyxml2::XML_SUCCESS) {
      std::string err_msg = "Error parse object type.";
      return Status(apollo::common::ErrorCode::HDMAP_DATA_ERROR, err_msg);
    }

    if (object_type == "stopline") {
      StopLineInternal stop_line;
      stop_line.id = object_id;
      PbCurveSegment* curve_segment = stop_line.curve.add_segment();
      const auto sub_node = object_node->FirstChildElement("geometry");
      RETURN_IF_ERROR(UtilXmlParser::parse_geometry(*sub_node,
                                                    curve_segment));
      stop_lines->emplace_back(stop_line);
    }
    object_node = object_node->NextSiblingElement("object");
  }
  return Status::OK();
}

}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
