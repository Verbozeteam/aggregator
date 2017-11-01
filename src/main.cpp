#include "config/config.hpp"
#include "logging/logging.hpp"
#include "logging/logging.hpp"
#include "socket_cluster/socket_cluster.hpp"
#include "aggregator_clients/aggregator_client.hpp"
#include "aggregator_clients/discovery_protocol.hpp"
#include "aggregator_clients/client_manager.hpp"
#include "verboze_api/verboze_api.hpp"

int main(int argc, char* argv[]) {
    if (ConfigManager::LoadFromCommandline(argc, argv) != 0)
        return -1;

    int retval = 0;

    if (Log::Initialize() == 0) {
        if (DiscoveryProtocol::Initialize() == 0) {
            if (SocketCluster::Initialize() == 0) {
                if (VerbozeAPI::Initialize() == 0) {
                    if (ClientManager::Initialize() == 0) {
                        LOG(info) << "Initialization done!";

                        SocketCluster::WaitForCompletion();

                        ClientManager::Cleanup();
                    } else
                        retval = -6;
                    VerbozeAPI::Cleanup();
                } else
                    retval = -5;
                SocketCluster::Cleanup();
            } else
                retval = -4;
            DiscoveryProtocol::Cleanup();
        } else
            retval = -3;
        Log::Cleanup();
    } else
        retval = -2;

    return retval;
}

