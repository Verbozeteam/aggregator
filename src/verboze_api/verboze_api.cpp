#include "config/config.hpp"
#include "logging/logging.hpp"
#include "verboze_api/verboze_api.hpp"

int VerbozeAPI::Initialize() {
    int ret = __initializeWebsockets();
    if (ret != 0)
        return ret;

    LOG(info) << "Verboze API initialized";
    return 0;
}

void VerbozeAPI::Cleanup() {
    __cleanupWebsockets();
}
