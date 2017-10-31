#pragma once

#include "socket_cluster/socket_cluster.hpp"

/**
 * The AggregatorClient extends SocketClient to add:
 * - Caching Thing states
 * - Heartbeats
 * - Forwarding from/to websockets
 */
class AggregatorClient : public SocketClient {
    friend class SocketClient;

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
};
