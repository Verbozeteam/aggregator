#include "config/config.hpp"
#include "logging/logging.hpp"
#include "logging/logging.hpp"
#include "socket_cluster/socket_cluster.hpp"
#include "aggregator_clients/aggregator_client.hpp"
#include "aggregator_clients/discovery_protocol.hpp"
#include "aggregator_clients/client_manager.hpp"

#include <time.h>

int main(int argc, char* argv[]) {
    if (ConfigManager::LoadFromCommandline(argc, argv) != 0)
        return -1;

    if (Log::Initialize() != 0)
        return -2;

    if (DiscoveryProtocol::Initialize() != 0)
        return -3;

    if (SocketCluster::Initialize() != 0)
        return -4;

    if (ClientManager::Initialize() != 0)
        return -5;

    // SocketClientPtr sc = SocketClient::Create<AggregatorClient> ("10.11.28.41", 4567);
    // if (sc)
    //     sc->Write("{\"code\": 0}"_json);
    // sleep(2);
    // SocketCluster::DeregisterClient(sc);
    // sc.reset();
    // SocketCluster::Kill();

    class Kaboom : public DiscoveryCallback {
    public:
        void OnDeviceDiscovered(std::string interface, std::string ip, std::string name, int type, std::string data) {
            LOG(trace) << ip << " " << name << " " << type << " " << data;
        }
    } kaboom;
    DiscoveryProtocol::InitiateDiscovery(&kaboom);

    SocketCluster::WaitForCompletion();

    ClientManager::Cleanup();

    SocketCluster::Cleanup();

    DiscoveryProtocol::Cleanup();

    Log::Cleanup();

    return 0;
}
