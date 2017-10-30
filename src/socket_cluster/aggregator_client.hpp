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

    virtual void Write(json msg);
    virtual bool OnMessage(json msg);
};
