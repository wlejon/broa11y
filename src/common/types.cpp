#include "broa11y/types.h"

namespace broa11y {

std::string_view relation_type_to_string(RelationType relation) {
    switch (relation) {
        case RelationType::ControlledBy: return "controlled_by";
        case RelationType::ControllerFor: return "controller_for";
        case RelationType::DescribedBy: return "described_by";
        case RelationType::DescriptionFor: return "description_for";
        case RelationType::LabelledBy: return "labelled_by";
        case RelationType::LabelFor: return "label_for";
        case RelationType::MemberOf: return "member_of";
        case RelationType::NodeChildOf: return "node_child_of";
        case RelationType::FlowsTo: return "flows_to";
        case RelationType::FlowsFrom: return "flows_from";
        case RelationType::SubwindowOf: return "subwindow_of";
    }
    return "unknown";
}

} // namespace broa11y
