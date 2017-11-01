#pragma once

#include <cpprest/ws_client.h>
using namespace web::websockets::client;

#include <json.hpp>
using json = nlohmann::json;

typedef void (*CommandCallback) (json command);

/**
 * This class is responsible for providing the tools to interface with the Verboze server
 */
class VerbozeAPI {
    static CommandCallback m_command_callback;
    static websocket_callback_client m_permenanat_client;

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
