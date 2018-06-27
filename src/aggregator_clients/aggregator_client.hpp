#pragma once

#include "socket_cluster/socket_cluster.hpp"
#include "aggregator_clients/discovery_protocol.hpp"

#include <vector>
#include <string>

#include <json.hpp>
using json = nlohmann::json;

/**
 * The AggregatorClient extends SocketClient to add:
 * - Caching Thing states
 * - Heartbeats
 * - Forwarding from/to websockets
 */
class AggregatorClient : public SocketClient {
    friend class SocketClient;
    friend class ClientManager;

    /** Cache of the state of the client */
    json m_cache;

    /** Client room id */
    std::string m_room_id;

    /** Discovery info */
    DISCOVERED_DEVICE m_discovery_info;

protected:
    AggregatorClient(int fd, std::string ip, int port);

public:

    /**
     * Extend the SocketClient Write function to make it cache messages
     */
    virtual void Write(json msg);

    /**
     * Extend the SocketClient OnMessage function to make it cache messages
     */
    virtual bool OnMessage(json msg);

    /**
     * Retrieves the cache of this client
     * @return Cache of the client
     */
    json GetCache(std::string key = "") const;

    /**
     * Retrieves the room ID of this client
     * @return m_room_id
     */
    std::string GetID() const;
};
