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

#ifndef MODULES_ROUTING_CORE_NAVIGATOR_H_
#define MODULES_ROUTING_CORE_NAVIGATOR_H_

#include <memory>

#include "modules/routing/core/node_range_manager.h"
#include "modules/routing/proto/routing.pb.h"

#include <unordered_set>

namespace apollo {
namespace routing {

class TopoGraph;
class TopoNode;

class Navigator {
 public:
  explicit Navigator(const std::string& topo_file_path);
  ~Navigator();
  bool is_ready() const;

  // search new request to new response
  bool search_route(const ::apollo::routing::RoutingRequest& request,
                    ::apollo::routing::RoutingResponse* response) const;

 private:
  // new request to new response
  bool generate_passage_region(
      const ::apollo::routing::RoutingRequest& request,
      const std::vector<const TopoNode*>& nodes,
      const std::unordered_set<const TopoNode*>& black_list,
      NodeRangeManager* const range_manager,
      ::apollo::routing::RoutingResponse* result) const;

  // use internal generate result
  void generate_passage_region(
      const std::vector<const TopoNode*>& nodes,
      const std::unordered_set<const TopoNode*>& black_list,
      NodeRangeManager* const range_manager,
      ::apollo::routing::RoutingResponse* result) const;

  void dump_debug_data(
      const std::vector<const TopoNode*>& nodes,
      const NodeRangeManager& range_manager,
      const ::apollo::routing::RoutingResponse& response) const;

 private:
  bool _is_ready;
  std::unique_ptr<TopoGraph> graph_;
};

}  // namespace routing
}  // namespace apollo

#endif  // MODULES_ROUTING_CORE_NAVIGATOR_H_
