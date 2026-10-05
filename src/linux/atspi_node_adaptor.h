#pragma once

#include "atspi_types.h"
#include "broa11y/tree.h"

#include <string>
#include <string_view>
#include <vector>

namespace broa11y::atspi {

class NodeAdaptor {
public:
    static std::string node_id_to_path(NodeId id, NodeId root_id = kRootNodeId);
    static NodeId path_to_node_id(std::string_view path, NodeId root_id = kRootNodeId);

    static Reference make_reference(std::string_view bus_name, NodeId id, NodeId root_id = kRootNodeId);

    static MethodReply handle_call(Tree* tree,
                                  std::string_view bus_name,
                                  const MethodCall& call);

private:
    static MethodReply handle_accessible(const Node* node,
                                        Tree* tree,
                                        std::string_view bus_name,
                                        std::string_view member,
                                        const std::vector<std::string>& args);

    static MethodReply handle_component(Node* node,
                                       Tree* tree,
                                       std::string_view bus_name,
                                       std::string_view member,
                                       const std::vector<std::string>& args);

    static MethodReply handle_action(Node* node,
                                    std::string_view member,
                                    const std::vector<std::string>& args);

    static MethodReply handle_text(Node* node,
                                  std::string_view member,
                                  const std::vector<std::string>& args);

    static MethodReply handle_editable_text(Node* node,
                                           std::string_view member,
                                           const std::vector<std::string>& args);

    static MethodReply handle_value(Node* node,
                                   std::string_view member,
                                   const std::vector<std::string>& args);
};

} // namespace broa11y::atspi
