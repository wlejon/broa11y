#pragma once

#include "embed/embed.h"
#include "object_builder.h"
#include "broa11y/broa11y.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace broa11y::api {

namespace ev = bronze::embed;
using Value = bronze::Value;

// Service accessors
std::shared_ptr<broa11y::Tree> activeTree();
void setTree(std::shared_ptr<broa11y::Tree> tree);
std::shared_ptr<broa11y::Tree> getTree();

std::shared_ptr<broa11y::Bridge> activeBridge();
void setBridge(std::shared_ptr<broa11y::Bridge> bridge);
std::shared_ptr<broa11y::Bridge> getBridge();

void ensureTreeListener(broa11y::Tree* tree);
void removeTreeListener();

// String/enum converters
Role parseRole(std::string_view str);
State parseState(std::string_view str);
std::string roleToString(Role role);
std::string stateToString(State state);

// Node representation converter
Value nodeToJs(const broa11y::Node* node);

// Module installers
void installTreeOnto(Value a11yObj);
void installAnnounceOnto(Value a11yObj);
void installCustomOnto(Value a11yObj);

// Event queue and listeners
void queueA11yEvent(const broa11y::Event& event);
void drainA11yEvents();
void clearA11yListeners();
void clearCustomRoles();

// Custom roles / widget hooks support
bool dispatchWidgetAction(NodeId id, std::string_view action, const ActionParams& params);

} // namespace broa11y::api
