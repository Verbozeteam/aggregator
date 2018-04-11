#pragma once

#include <string>

#include <json.hpp>
using json = nlohmann::json;

typedef void (*CommandCallback) (json command);

/**
 * This class is responsible for providing the tools to interface with the Verboze server
 */
class VerbozeAPI {
    /**
     * Initializes websocket stuff
     */
    static int __initializeWebsockets();
    /**
     * Cleans up websocket stuff
     */
    static void __cleanupWebsockets();
    /**
     * Entry point for a thread that will attempt to connect the websocket
     */
    static void __connect_websocket_async();

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
