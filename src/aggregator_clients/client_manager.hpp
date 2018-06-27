#pragma once

#include "aggregator_clients/aggregator_client.hpp"
#include "aggregator_clients/discovery_protocol.hpp"

#include <string>
#include <unordered_map>
#include <thread>

#include <json.hpp>
using json = nlohmann::json;

/** Period for discovery requests */
#define DISCOVERY_PERIOD 30000
/** Period for heartbeats */
#define HEARTBEAT_PERIOD 8000

/** Control codes */
#define CONTROL_CODE_GET_BLUEPRINT      0
#define CONTROL_CODE_GET_THING_STATE    1
#define CONTROL_CODE_SET_LISTENERS      2

class ClientManager {
    /** Holds authentication  */
    struct AUTHENTICATION_STRUCT {
        /** Authentication token */
        std::string token;
        /** One-time-use password */
        std::string password;

        AUTHENTICATION_STRUCT(std::string tok) : token(tok), password("") {}

        /**
         * Retrieves the json used for authentication message and clears the stored password
         */
        json get_json_and_clear_password() {
            std::string pw = password;
            password = "";
            return {
                {"authentication", {
                    {"token", token},
                    {"password", pw},
                }},
            };
        }
    };

    /** When false, the manager thread quits ASAP */
    static bool m_is_alive;
    /** Manager thread that maintains connection to middlewares */
    static std::thread m_manager_thread;
    /** Stored authentication credentials */
    static std::unordered_map<std::string, AUTHENTICATION_STRUCT> m_credentials_map;
    /** Clients that need a password to authenticate */
    static std::unordered_map<std::string, DISCOVERED_DEVICE> m_clients_require_password;

    /**
     * Checks whether or not authentication can be made to a client
     * @param dev  Client discovered device
     * @return     true iff an authentication attempt to dev is possible
     */
    static bool __clientCanAuthenticate(DISCOVERED_DEVICE dev);

    /**
     * Sends an authentication message to a given client using credentials from
     * m_credentials_map. If no credentials exist, a new token will be generated.
     * @param client  Client to authenticate
     */
    static void __authenticateClient(AggregatorClient* client);

    /**
     * Creates a string that is the key of a given client in m_credentials_map.
     * @param client  Discovered device info of client to generate a key for
     * @return        Generated key
     */
    static std::string __getClientCredentialsMapKey(DISCOVERED_DEVICE dev);

    /**
     * Mark a client that it needs a password to authenticate. If there is a default
     * password set, the function will use it immediately and put it in m_credentials_map
     * for the given client and return true.
     * @param client  Discovered device info of client
     * @return        true iff the client has a password ready in m_credentials_map
     */
    static bool __markClientRequiresPassword(DISCOVERED_DEVICE dev);

    /**
     * Reads m_credentials_map from file (filename in config)
     */
    static void __readCredentialsMap();

    /**
     * Writes m_credentials_map from file (filename in config)
     */
    static void __writeCredentialsMap();

    /**
     * Generates a new authentication token and stores it in m_credentials_map and
     * in the credentials file (if provided).
     * @param client  Client to associate the token with
     * @return        Newly generated authentication token
     */
    static std::string __generateNewAuthenticationToken(AggregatorClient* client);

    /**
     * Called by the discovery system (from another thread) when a device is discovered.
     * @param dev  Discovered device info
     */
    static void __onDeviceDiscovered(DISCOVERED_DEVICE dev);

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

    /**
     * Removes any credentials stored for a given client
     * @param client  Client to remove its credentials
     */
    static void RemoveClientCredentials(AggregatorClient* client);
};

