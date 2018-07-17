#include "config/config.hpp"
#include "logging/logging.hpp"
#include "aggregator_clients/client_manager.hpp"
#include "aggregator_clients/discovery_protocol.hpp"
#include "socket_cluster/socket_cluster.hpp"
#include "verboze_api/verboze_api.hpp"
#include "utilities/time_utilities.hpp"

#include <fstream>

bool ClientManager::m_is_alive = true;
std::thread ClientManager::m_manager_thread;
std::unordered_map<std::string, ClientManager::AUTHENTICATION_STRUCT> ClientManager::m_credentials_map;
std::unordered_map<std::string, DISCOVERED_DEVICE> ClientManager::m_clients_require_password;

void ClientManager::__onDeviceDiscovered(DISCOVERED_DEVICE dev) {
    // If the middleware on that IP is not registered, attempt to register it
    if (dev.type == 3) { // type 3 is a room
        if (!SocketCluster::IsClientRegistered(dev.ip+":"+std::to_string(dev.port)) && __clientCanAuthenticate(dev)) {
            SocketClientPtr sc = SocketClient::Create<AggregatorClient> (dev.ip, dev.port);
            if (sc) {
                AggregatorClient* ac = (AggregatorClient*)sc.get();
                ac->m_discovery_info = dev;
                __authenticateClient(ac); // authenticate
                sc->Write("{\"code\": 0}"_json); // Request blueprint
            }
        }
    }
}

void ClientManager::OnControlCommandFromAggregatorClient(AggregatorClient* client_from, json command) {
    std::string client_name = client_from->GetID();
    if (command.find("code") == command.end()) {
        LOG(warning) << "Received control command from " << client_name << " without code " << command.dump();
        return;
    }
    int code = command["code"];

    switch (code) {
        case CONTROL_CODE_RESET_QRCODE:
            // forward it to Verboze, stamp it so Verboze knows who sent it
            command["__reply_target"] = client_name;
            VerbozeAPI::SendCommand(command);
            break;

        case CONTROL_CODE_GET_BLUEPRINT:
        case CONTROL_CODE_GET_THING_STATE:
        case CONTROL_CODE_SET_LISTENERS:
        case CONTROL_CODE_SET_QRCODE:
            LOG(warning) << "Received unsupported control code " << code << " from " << client_name;
            break;
        default:
            LOG(warning) << "Received unknown control code " << code << " from " << client_name;
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
        } case CONTROL_CODE_RESET_QRCODE: {
            /** CANNOT BE IMPLEMENTED HERE! (This is only sent by clients...) */
        } case CONTROL_CODE_SET_QRCODE: {
            // forward the new QR code to the room
            if (command.find("qr-code") != command.end())
                command["qr-code"] = VerbozeAPI::TokenToStreamURL(command["qr-code"]);
            target_room->Write(command);
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

    // Load stored credentials
    __readCredentialsMap();

    m_manager_thread = std::thread(__threadEntry);

    VerbozeAPI::SetCommandCallback(__onCommandFromVerboze);

    return 0;
}

void ClientManager::Cleanup() {
    m_is_alive = false;

    if (m_manager_thread.joinable())
        m_manager_thread.join();
}

bool ClientManager::__clientCanAuthenticate(DISCOVERED_DEVICE dev) {
    std::string client_key = __getClientCredentialsMapKey(dev);
    auto it = m_credentials_map.find(client_key);
    if (it == m_credentials_map.end()) {
        std::string pw = ConfigManager::get<std::string>("credentials-password");
        if (pw != "") {
            // Generate a new token and set its password since we know what password to always use
            __generateNewAuthenticationToken(dev);
            m_credentials_map.find(client_key)->second.password = pw;
            return true;
        }
        // this client has no authentication info, add it to the m_clients_require_password (if it doesn't exist)
        return __markClientRequiresPassword(dev);
    }
    return true;
}

void ClientManager::__authenticateClient(AggregatorClient* client) {
    std::string client_key = __getClientCredentialsMapKey(client->m_discovery_info);
    auto iter = m_credentials_map.find(client_key);
    if (iter != m_credentials_map.end())
        client->Write(iter->second.get_json_and_clear_password()); // Authenticate
}

void ClientManager::RemoveClientCredentials(AggregatorClient* client) {
    std::string client_key = __getClientCredentialsMapKey(client->m_discovery_info);
    LOG(warning) << "Credentials for client " << client_key << " no longer works.";
    m_credentials_map.erase(client_key);
    __writeCredentialsMap();

    // add this client to the list of clients that require password to authenticate (since it failed - it needs a new token)
    __markClientRequiresPassword(client->m_discovery_info);
}

std::string ClientManager::__getClientCredentialsMapKey(DISCOVERED_DEVICE dev) {
    return dev.name + ":" + dev.ip + ":" + std::to_string(dev.port);
}

bool ClientManager::__markClientRequiresPassword(DISCOVERED_DEVICE dev) {
    std::string client_key = __getClientCredentialsMapKey(dev);
    if (m_clients_require_password.find(client_key) == m_clients_require_password.end()) {
        LOG(debug) << "Client " << dev.name << " (" << dev.ip << ":" << dev.port << ") requires password to authenticate";
        m_clients_require_password.insert(std::pair<std::string, DISCOVERED_DEVICE>(client_key, dev));
    }

    return false;
}

void ClientManager::__readCredentialsMap() {
    std::string filename = ConfigManager::get<std::string>("credentials-file");
    if(filename.size() > 0) {
        LOG(info) << "Using credentials file " << filename;
        std::fstream file;
        file.open(filename, std::ios::in);
        if (file.is_open()) {
            while (!file.eof()) {
                std::string identifier, token;
                if (std::getline(file, identifier)) {
                    if (std::getline(file, token)) {
                        m_credentials_map.insert(std::pair<std::string, ClientManager::AUTHENTICATION_STRUCT>(identifier, AUTHENTICATION_STRUCT(token)));
                    }
                }
            }
            file.close();
        } else
            LOG(warning) << "Failed to open credentials file " << filename;
    }
}

void ClientManager::__writeCredentialsMap() {
    std::string filename = ConfigManager::get<std::string>("credentials-file");
    if (filename.size() > 0) {
        std::fstream file;
        file.open(filename, std::ios::out);
        if (file.is_open()) {
            for (auto iter = m_credentials_map.begin(); iter != m_credentials_map.end(); iter++) {
                file << iter->first << std::endl;
                file << iter->second.token << std::endl;
            }
        } else
            LOG(warning) << "Failed to open credentials file " << filename;
    }
}

std::string ClientManager::__generateNewAuthenticationToken(DISCOVERED_DEVICE dev) {
    // letters a-z, A-Z, and numbers 0-9
    const int num_symbols = 26 * 2 + 10;
    const int token_length = 64;
    std::string token = "";

    for (int i = 0; i < token_length; i++) {
        int r = rand() % num_symbols;
        if (r < 26)
            token += (char)('a' + r);
        else if (r < 26 * 2)
            token += (char)('A' + (r-26));
        else
            token += (char)('0' + (r-26*2));
    }

    std::string key = __getClientCredentialsMapKey(dev);
    m_credentials_map.insert(std::pair<std::string, ClientManager::AUTHENTICATION_STRUCT>(key, AUTHENTICATION_STRUCT(token)));
    __writeCredentialsMap();

    return token;
}
