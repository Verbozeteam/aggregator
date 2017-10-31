#pragma once

#include <string>
#include <thread>

#define MIDDLEWARE_PORT 4567

class ClientManager {
    /** When false, the manager thread quits ASAP */
    static bool m_is_alive;
    /** Manager thread that maintains connection to middlewares */
    static std::thread m_manager_thread;

public:
    static int Initialize();

    static void Cleanup();
};

