#pragma once

#include <string>
#include <thread>

#define MIDDLEWARE_PORT 4567
#define DISCOVERY_PERIOD 10000
#define HEARTBEAT_PERIOD 8000

class ClientManager {
    /** When false, the manager thread quits ASAP */
    static bool m_is_alive;
    /** Manager thread that maintains connection to middlewares */
    static std::thread m_manager_thread;

    static void __onDeviceDiscovered(std::string interface, std::string name, std::string ip, int type, std::string data);

    static void __threadEntry();

public:
    static int Initialize();

    static void Cleanup();
};

