#pragma once

#include "socket_client.hpp"

#include <vector>
using std::vector;

class SocketServer {
    int m_server_fd;
    std::vector<SocketClient*> m_clients;

    static void __thread_entry(void* ptr);

public:
    SocketServer();
    ~SocketServer();

    int RunThread(int max_connections=1024);
};
