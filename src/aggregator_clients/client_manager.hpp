#pragma once

#include <string>
#include <thread>

#include <json.hpp>
using json = nlohmann::json;

/** Port that the middleware uses */
#define MIDDLEWARE_PORT 4567
/** Period for discovery requests */
#define DISCOVERY_PERIOD 10000
/** Period for heartbeats */
#define HEARTBEAT_PERIOD 8000

class ClientManager {
    /** When false, the manager thread quits ASAP */
    static bool m_is_alive;
    /** Manager thread that maintains connection to middlewares */
    static std::thread m_manager_thread;

    /**
     * Called by the discovery system (from another thread) when a device is discovered.
     * @param interface Interface on which the device was discovered
     * @param name      Name of the device
     * @param ip        IP of the device
     * @param type      Type of the device
     * @param data      Data of the device
     */
    static void __onDeviceDiscovered(std::string interface, std::string name, std::string ip, int type, std::string data);

    /**
     * Callback called by the VerbozeAPI when a command is sent
     * @param command The JSON command
     */
    static void __onCommandFromVerboze(json command);

    /**
     * Thread entry point
     */
    static void __threadEntry();

public:
    /**
     * Initializes the client management system
     * @return 0 on success, negative value on failure
     */
    static int Initialize();

    /**
     * Kills the thread and cleans up resources
     */
    static void Cleanup();
};

