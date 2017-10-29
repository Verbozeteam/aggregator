#pragma once

#include "socket_client.hpp"

#include <unordered_map>

#include <thread>

class SocketServer {
    static int m_hosting_port;
    static std::unordered_map<int, SocketClient*> m_clients; // fd -> SocketClient* map
    static std::thread m_server_thread;

    static int __openHostingSocket(int max_connections);
    static void __thread_entry(int max_connections);

    static void RegisterClient(SocketClient* client);
    static void DeregisterClient(SocketClient* client);

public:
    static int Initialize();
    static void WaitForCompletion();
};
