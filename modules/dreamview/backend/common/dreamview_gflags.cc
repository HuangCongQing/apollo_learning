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

#include "modules/dreamview/backend/common/dreamview_gflags.h"

DEFINE_string(dreamview_module_name, "dreamview", "dreamview module name");

DEFINE_string(dreamview_adapter_config_filename,
              "modules/dreamview/conf/adapter.conf", "The adapter config file");

DEFINE_string(hmi_config_filename, "modules/dreamview/conf/hmi.conf",
              "The HMI config file");

DEFINE_string(static_file_dir, "modules/dreamview/frontend/dist",
              "The path to the dreamview distribution directory. The default "
              "value points to built-in version from the Apollo project.");

DEFINE_string(server_ports, "8888",
              "Comma-separated list of ports to listen on. If the port is SSL, "
              "a letter s must be appended, for example, 80,443s will open "
              "port 80 and port 443.");

DEFINE_bool(enable_sim_control, false,
            "Whether to enable SimControl to publish localization and chassis "
            "message.");

DEFINE_bool(routing_from_file, false,
            "Whether Dreamview reads initial routing response from file.");

DEFINE_string(routing_response_file,
              "modules/map/data/demo/garage_routing.pb.txt",
              "File path of the routing response that SimControl will read the "
              "start point from. If this is absent, SimControl will directly "
              "take the RoutingResponse from ROS to determine the start "
              "point.");

DEFINE_string(websocket_timeout_ms, "36000000",
              "Time span that CivetServer keeps the websocket connection alive "
              "without dropping it.");

DEFINE_string(ssl_certificate, "",
              "Path to the SSL certificate file. This option is only required "
              "when at least one of the listening_ports is SSL. The file must "
              "be in PEM format, and it must have both, private key and "
              "certificate");

DEFINE_double(sim_map_radius, 300.0,
              "The radius within which Dreamview will find all the map "
              "elements around the car.");
