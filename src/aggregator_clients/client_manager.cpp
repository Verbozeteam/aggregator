#include "logging/logging.hpp"
#include "aggregator_clients/client_manager.hpp"
#include "aggregator_clients/aggregator_client.hpp"
#include "aggregator_clients/discovery_protocol.hpp"
#include "socket_cluster/socket_cluster.hpp"
#include "verboze_api/verboze_api.hpp"
#include "utilities/time_utilities.hpp"

bool ClientManager::m_is_alive = true;
std::thread ClientManager::m_manager_thread;

void ClientManager::__onDeviceDiscovered(std::string interface, std::string name, std::string ip, int port, int type, std::string data) {
    // If the middleware on that IP is not registered, attempt to register it
    if (type == 3) { // type 3 is a room
        if (!SocketCluster::IsClientRegistered(ip+":"+std::to_string(port))) {
            SocketClientPtr sc = SocketClient::Create<AggregatorClient> (ip, port);
            if (sc)
                sc->Write("{\"code\": 0}"_json);
        }
    }
}

void ClientManager::__onCommandFromVerboze(json command) {
    std::string room_name = "";
    auto command_it = command.find("__room_name");
    if (command_it != command.end()) {
        room_name = command_it.value();
        command.erase("__room_name");
    }

    std::vector<SocketClientPtr> all_clients = SocketCluster::GetClientsList();
    for (auto it : all_clients) {
        AggregatorClient* cl = (AggregatorClient*)(it.get());
        if (cl->HasRoom(room_name))
            cl->Write(command);
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

    VerbozeAPI::SetCommandCallback(__onCommandFromVerboze);

    return 0;
}

void ClientManager::Cleanup() {
    m_is_alive = false;

    if (m_manager_thread.joinable())
        m_manager_thread.join();
}
