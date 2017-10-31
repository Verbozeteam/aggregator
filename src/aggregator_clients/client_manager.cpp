#include "logging/logging.hpp"
#include "aggregator_clients/client_manager.hpp"
#include "aggregator_clients/aggregator_client.hpp"
#include "aggregator_clients/discovery_protocol.hpp"


void DeviceDiscovered(std::string interface, std::string name, std::string ip, int type, std::string data) {
    SocketClientPtr sc = SocketClient::Create<AggregatorClient> (ip, MIDDLEWARE_PORT);
    if (sc)
        sc->Write("{\"code\": 0}"_json);
    // sleep(2);
    // SocketCluster::DeregisterClient(sc);
    // sc.reset();
}

int ClientManager::Initialize() {
    DiscoveryProtocol::InitiateDiscovery(&DeviceDiscovered);

    return 0;
}

void ClientManager::Cleanup() {

}
