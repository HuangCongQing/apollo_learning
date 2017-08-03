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

#include "modules/routing/routing.h"
#include "modules/routing/core/navigator.h"
#include "modules/common/adapters/adapter_gflags.h"
#include "modules/common/adapters/adapter_manager.h"
#include "modules/routing/common/routing_gflags.h"

namespace apollo {
namespace routing {

using ::apollo::common::adapter::AdapterManager;
using ::apollo::common::monitor::MonitorMessageItem;
using apollo::common::ErrorCode;

std::string Routing::Name() const { return FLAGS_node_name; }

Routing::Routing() :
  monitor_(apollo::common::monitor::MonitorMessageItem::ROUTING) {

  std::string graph_path = FLAGS_graph_dir + "/" + FLAGS_graph_file_name;
  AINFO << "Use routing topology graph path: " <<  graph_path.c_str();
  _navigator_ptr.reset(new Navigator(graph_path));
}

apollo::common::Status Routing::Init() {
  std::string graph_path = FLAGS_graph_dir + "/" + FLAGS_graph_file_name;

  AERROR << "debug 1";
  AdapterManager::Init(FLAGS_adapter_config_path);
  AERROR << "debug 2";
  AdapterManager::AddMonitorCallback(&Routing::OnMonitor, this);
  AERROR << "debug 3";
  AdapterManager::AddRoutingRequestCallback(&Routing::OnRouting_Request, this);
  AERROR << "debug 4";

  return apollo::common::Status::OK();
}

apollo::common::Status Routing::Start() {
  if (!_navigator_ptr->is_ready()) {
    AERROR << "Navigator is not ready!";
    return apollo::common::Status(ErrorCode::ROUTING_ERROR, "Navigator not ready");
  }
  AERROR << "Routing service is ready.";

  apollo::common::monitor::MonitorBuffer buffer(&monitor_);
  buffer.INFO("Routing started");
  return apollo::common::Status::OK();
}

void Routing::OnRouting_Request(const apollo::routing::RoutingRequest &routing_request) {
  AINFO << "Get new routing request!!!";
  ::apollo::routing::RoutingResponse routing_response;
  if (!_navigator_ptr->search_route(routing_request, &routing_response)) {
    AERROR << "Failed to search route with navigator.";
    return;
  }

  //AdapterManager::FillRoutingResponseHeader(Name(), routing_response);
  AdapterManager::PublishRoutingResponse(routing_response);
  return;
}

void Routing::Stop() {}

void Routing::OnMonitor(
    const apollo::common::monitor::MonitorMessage &monitor_message) {
  for (const auto &item : monitor_message.item()) {
    if (item.log_level() == MonitorMessageItem::FATAL) {
      return;
    }
  }
}

}  // namespace routing
}  // namespace apollo
