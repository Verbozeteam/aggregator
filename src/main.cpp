#include <config/config.hpp>
#include <logging/logging.hpp>
#include <logging/logging.hpp>
#include <socket_server/socket_server.hpp>

int main(int argc, char* argv[]) {
    if (ConfigManager::LoadFromCommandline(argc, argv) != 0)
        return 1;

    if (Log::Initialize() != 0)
        return 2;

    Log::Cleanup();

    return 0;
}
