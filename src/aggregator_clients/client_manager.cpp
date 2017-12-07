#include "logging/logging.hpp"
#include "aggregator_clients/client_manager.hpp"
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

void ClientManager::__onControlCommandFromVerboze(json command, int code, AggregatorClient* target_room) {
    auto reply_target = command.find("__reply_target");

    switch (code) {
        case CONTROL_CODE_GET_BLUEPRINT: {
            json blueprint = target_room->GetCache();
            blueprint["__room_id"] = target_room->GetID();
            if (reply_target != command.end())
                blueprint["__reply_target"] = command["__reply_target"];
            VerbozeAPI::SendCommand(blueprint);
            break;
        } case CONTROL_CODE_GET_THING_STATE: {
            if (command.find("thing-id") != command.end()) {
                std::string thing_id = command["thing-id"];
                json thing_state = target_room->GetCache(thing_id);
                if (!thing_state.is_null()) {
                    thing_state["thing"] = thing_id;
                    thing_state["__room_id"] = target_room->GetID();
                    if (reply_target != command.end())
                        thing_state["__reply_target"] = command["__reply_target"];
                    VerbozeAPI::SendCommand(thing_state);
                }
            }
            break;
        } case CONTROL_CODE_SET_LISTENERS: {
            /** CANNOT BE IMPLEMENTED HERE! (Verboze listens to all) */
            break;
        }
    }
}

void ClientManager::__onCommandFromVerboze(json command) {
    auto command_it = command.find("__room_id");
    if (command_it != command.end()) {
        std::string room_id = command_it.value();
        command.erase("__room_id");

        AggregatorClient* target_room = nullptr;
        std::vector<SocketClientPtr> all_clients = SocketCluster::GetClientsList();
        for (auto it : all_clients) {
            AggregatorClient* cl = (AggregatorClient*)(it.get());
            if (cl->GetID() == room_id) {
                target_room = cl;
                break;
            }
        }

        if (target_room) {
            if (command.find("thing") == command.end()) {
                // control command, handle it now
                auto code_iter = command.find("code");
                if (code_iter != command.end()) {
                    int code = code_iter.value();
                    __onControlCommandFromVerboze(command, code, target_room);
                }
                return;
            } else {
                // state update, just forward it to the respective middleware
                target_room->Write(command);
            }
        }
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
