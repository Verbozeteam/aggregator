#include "logging/logging.hpp"
#include "aggregator_clients/aggregator_client.hpp"
#include "aggregator_clients/client_manager.hpp"
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
                (*base)[key] = val;
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

    /** Perform caching (REMOVED) */
    //__merge_json(&m_cache, msg);
}

bool AggregatorClient::OnMessage(json msg) {
    SocketClient::OnMessage(msg);

    if (msg.find("noauth") != msg.end()) {
        LOG(warning) << "Middleware rejected client " << m_identifier << " for no authentication";
        ClientManager::RemoveClientCredentials(this);
        return false;
    }

    if (msg.find("code") != msg.end()) {
        ClientManager::OnControlCommandFromAggregatorClient(this, msg);
        return true;
    }

    if (msg.find("thing") != msg.end()) // don't react to thing controls messages (should never happen...)
        return true;

    /** Perform caching */
    bool changed_state = __merge_json(&m_cache, msg);

    std::string old_room_id = m_room_id;
    if (msg.find("config") != msg.end()) {
        try {
            m_room_id = msg["config"]["id"];
        } catch (...) {
            m_room_id = "";
        }
    }
    if (old_room_id != m_room_id && m_room_id != "") {
        VerbozeAPI::Endpoints::RegisterRoom(
            m_room_id,
            m_discovery_info.name,
            m_discovery_info.interface,
            m_discovery_info.ip,
            m_discovery_info.port,
            m_discovery_info.type,
            m_discovery_info.data
        );
    }

    if (changed_state) {
        /** Put the __room_names stamp on the message */
        msg["__room_id"] = m_room_id;
        VerbozeAPI::SendCommand(msg);
    }

    return true;
}

json AggregatorClient::GetCache(std::string key) const {
    if (key == "")
        return m_cache;
    else {
        auto it = m_cache.find(key);
        if (it != m_cache.end())
            return it.value();
        return json();
    }
}

std::string AggregatorClient::GetID() const {
    return m_room_id;
}
