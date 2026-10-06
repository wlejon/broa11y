#pragma once

#include "broa11y/action.h"
#include "broa11y/bridge.h"
#include "broa11y/events.h"
#include "broa11y/node.h"
#include "broa11y/role.h"
#include "broa11y/state.h"
#include "broa11y/terminal.h"
#include "broa11y/tree.h"
#include "broa11y/types.h"
#include "broa11y/version.h"

// The bridge for this platform; the others are not built here.
#if defined(_WIN32)
#include "broa11y/win_bridge.h"
#elif defined(__APPLE__)
#include "broa11y/mac_bridge.h"
#elif defined(__linux__)
#include "broa11y/linux_bridge.h"
#endif
