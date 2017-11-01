#include "logging/logging.hpp"
#include "aggregator_clients/aggregator_client.hpp"

void __merge_json(json* base, json new_data) {
    if (new_data.is_object()) {
        for (json::iterator it = new_data.begin(); it != new_data.end(); it++) {
            std::string key = it.key();
            json val = it.value();

            auto existing_entry = base->find(key);
            if (existing_entry != base->end()) {
                json* sub_base = &existing_entry.value();
                if (sub_base->is_object() && val.is_object())
                    __merge_json(sub_base, val);
                else
                    base->emplace(key, val);
            } else
                base->emplace(key, val);
        }
    }
}

AggregatorClient::AggregatorClient(int fd, std::string ip) : SocketClient(fd, ip) {
}

void AggregatorClient::Write(json msg) {
    SocketClient::Write(msg); // actually writes to the buffer

    /** Perform caching */
    __merge_json(&m_cache, msg);
}

bool AggregatorClient::OnMessage(json msg) {
    SocketClient::OnMessage(msg);

    /** Perform caching */
    __merge_json(&m_cache, msg);

    return true;
}
