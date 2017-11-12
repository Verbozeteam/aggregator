#pragma once

#include "aggregator_clients/aggregator_client.hpp"

#include <string>
#include <thread>

#include <json.hpp>
using json = nlohmann::json;

/** Period for discovery requests */
#define DISCOVERY_PERIOD 10000
/** Period for heartbeats */
#define HEARTBEAT_PERIOD 8000

/** Control codes */
#define CONTROL_CODE_GET_BLUEPRINT      0
#define CONTROL_CODE_GET_THING_STATE    1
#define CONTROL_CODE_SET_LISTENERS      2

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
     * @param port      Port on which the device hosts the service
     * @param type      Type of the device
     * @param data      Data of the device
     */
    static void __onDeviceDiscovered(std::string interface, std::string name, std::string ip, int port, int type, std::string data);

    /**
     * Callback called by the VerbozeAPI when a command is sent
     * @param command The JSON command
     */
    static void __onCommandFromVerboze(json command);

    /**
     * Responds to a control command from Verboze
     * @param command      Control command sent by Verboze
     * @param code         Control code
     * @param target_room  Pointer to the target room
     */
    static void __onControlCommandFromVerboze(json command, int code, AggregatorClient* target_room);

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

