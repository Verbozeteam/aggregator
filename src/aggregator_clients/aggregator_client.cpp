#include "logging/logging.hpp"
#include "aggregator_clients/aggregator_client.hpp"
#include "verboze_api/verboze_api.hpp"

/**
 * Adds JSON data into an existing JSON dictionary recursing on OBJECT types
 * @param  base     The base JSON object that will be modified (type must be OBJECT)
 * @param  new_data The data to merge into base (type must be OBJECT)
 * @return          true if anything changed, false otherwise
 */
bool __merge_json(json* base, json new_data) {
    bool is_changed = false;
    if (new_data.is_object()) {
        for (json::iterator it = new_data.begin(); it != new_data.end(); it++) {
            std::string key = it.key();
            json val = it.value();

            auto existing_entry = base->find(key);
            bool replace_entry = false;
            if (existing_entry != base->end()) {
                json* sub_base = &existing_entry.value();
                if (sub_base->is_object() && val.is_object())
                    is_changed = is_changed || __merge_json(sub_base, val);
                else if (val != *sub_base)
                    replace_entry = true;
            } else
                replace_entry = true;

            if (replace_entry) {
                base->emplace(key, val);
                is_changed = true;
            }
        }
    }
    return is_changed;
}

AggregatorClient::AggregatorClient(int fd, std::string ip, int port) : SocketClient(fd, ip, port) {
}

void AggregatorClient::Write(json msg) {
    SocketClient::Write(msg); // actually writes to the buffer

    /** Perform caching */
    __merge_json(&m_cache, msg);
}

bool AggregatorClient::OnMessage(json msg) {
    SocketClient::OnMessage(msg);

    /** Perform caching */
    bool changed_state = __merge_json(&m_cache, msg);

    if (msg.find("config") != msg.end()) {
        m_room_names.clear();
        try {
            json rooms = msg["config"]["rooms"];
            for (json::iterator it = rooms.begin(); it != rooms.end(); it++) {
                json room = it.value();
                if (room.find("name") != room.end()) {
                    json names = room["name"];
                    if (names.find("en") != names.end())
                        m_room_names.push_back(names["en"]);
                }
            }
        } catch (...) {}
    }

    if (changed_state)
        VerbozeAPI::SendCommand(msg);

    return true;
}

bool AggregatorClient::HasRoom(std::string name) {
    for (auto it = m_room_names.begin(); it != m_room_names.end(); it++)
        if (*it == name)
            return true;
    return false;
}
