#pragma once

#include "socket_cluster/socket_cluster.hpp"

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

    /** Client name */
    std::vector<std::string> m_room_names;

protected:
    AggregatorClient(int fd, std::string ip);

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
     * Checks if this client has a room with a given name
     * @param  name Name to look for
     * @return      whether or not a room with the given name exists on this client
     */
    bool HasRoom(std::string name);
};
