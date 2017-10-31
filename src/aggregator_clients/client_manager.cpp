#include "logging/logging.hpp"
#include "aggregator_clients/client_manager.hpp"
#include "aggregator_clients/aggregator_client.hpp"
#include "aggregator_clients/discovery_protocol.hpp"
#include "socket_cluster/socket_cluster.hpp"
#include "utilities/time_utilities.hpp"

bool ClientManager::m_is_alive = true;
std::thread ClientManager::m_manager_thread;

void ClientManager::__onDeviceDiscovered(std::string interface, std::string name, std::string ip, int type, std::string data) {
    // If the middleware on that IP is not registered, attempt to register it
    if (!SocketCluster::IsClientRegistered(ip)) {
        SocketClientPtr sc = SocketClient::Create<AggregatorClient> (ip, MIDDLEWARE_PORT);
        if (sc)
            sc->Write("{\"code\": 0}"_json);
    }
}

void ClientManager::__threadEntry() {
    milliseconds cur_time = __get_time_ms();

    milliseconds next_heartbeat_round = cur_time;
    milliseconds next_discovery_round = cur_time;

    while (m_is_alive) {
        if (cur_time >= next_discovery_round - milliseconds(100)) {
            next_discovery_round = cur_time + milliseconds(DISCOVERY_PERIOD);

            DiscoveryProtocol::InitiateDiscovery(&__onDeviceDiscovered);
        }

        if (cur_time >= next_heartbeat_round - milliseconds(100)) {
            next_heartbeat_round = cur_time + milliseconds(HEARTBEAT_PERIOD);

            std::vector<SocketClientPtr> all_clients = SocketCluster::GetClientsList();
            for (auto it : all_clients)
                it->Write(json::object());
        }

        milliseconds sleep_time = std::min(next_heartbeat_round - cur_time,
                                           next_discovery_round - cur_time);

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

        cur_time = __get_time_ms();
    }
}

int ClientManager::Initialize() {
    m_is_alive = true;

    m_manager_thread = std::thread(__threadEntry);

    return 0;
}

void ClientManager::Cleanup() {
    m_is_alive = false;

    if (m_manager_thread.joinable())
        m_manager_thread.join();
}
