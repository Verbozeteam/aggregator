#pragma once

#include <string>

#include <cpprest/ws_client.h>
using namespace web::websockets::client;

#include <json.hpp>
using json = nlohmann::json;

typedef void (*CommandCallback) (json command);

/**
 * This class is responsible for providing the tools to interface with the Verboze server
 */
class VerbozeAPI {
    /** URL that the websocket will try to connect to */
    static std::string m_connection_url;
    /** The client used for websocket communication */
    static websocket_callback_client m_permenanat_client;
    /** Callback to be called when a message arrives from the websocket */
    static CommandCallback m_command_callback;

    /**
     * Attempts to connect the websocket to m_connection_url
     * @param  num_attempts Maximum number of attempts (-ve is infinite)
     * @return              0 on successfull connection, negative otherwise
     */
    static int __reconnect(int num_attempts);

public:
    /**
     * Initializes the API
     * @return 0 on success, negative value on failure
     */
    static int Initialize();

    /**
     * Shut down and clean up resources
     */
    static void Cleanup();

    /**
     * Send a command over websockets
     */
    static void SendCommand(json command);

    /**
     * Sets the callback to be called when a command is received over websockets from Verboze
     * @param callback Function to be called when a command is received
     */
    static void SetCommandCallback(CommandCallback callback);
};
