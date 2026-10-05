#include "broa11y/bridge.h"
#include "broa11y/linux_bridge.h"
#include "broa11y/mac_bridge.h"
#include "broa11y/win_bridge.h"

namespace broa11y {

std::unique_ptr<Bridge> create_platform_bridge(Tree* tree) {
#if defined(__APPLE__)
    return create_mac_bridge(tree);
#elif defined(_WIN32)
    return create_win_bridge(tree);
#else
    return create_linux_bridge(tree);
#endif
}

} // namespace broa11y
