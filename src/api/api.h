#pragma once

#include <memory>

namespace broa11y {
class Tree;
class Bridge;
}

namespace broa11y::api {

/// Mounts `bro.a11y` in the current Bronze realm.
void installA11y();

/// Pumps async accessibility events and dispatches JS listeners.
void tickA11yAsync();

/// Cleans up active listeners and hooks.
void shutdownA11yAsync();

/// Checks whether accessibility platform infrastructure is available.
bool available();

/// Sets the Tree used by the API (if nullptr, uses default tree).
void setTree(std::shared_ptr<broa11y::Tree> tree);

/// Gets the Tree currently used by the API.
std::shared_ptr<broa11y::Tree> getTree();

/// Sets the Bridge used by the API.
void setBridge(std::shared_ptr<broa11y::Bridge> bridge);

/// Gets the Bridge currently used by the API.
std::shared_ptr<broa11y::Bridge> getBridge();

} // namespace broa11y::api

using broa11y::api::installA11y;
using broa11y::api::tickA11yAsync;
using broa11y::api::shutdownA11yAsync;
using broa11y::api::available;
using broa11y::api::setTree;
using broa11y::api::getTree;
using broa11y::api::setBridge;
using broa11y::api::getBridge;
