#pragma once

#include <cpprest/ws_client.h>
using namespace web;
using namespace web::websockets::client;

#include <json.hpp>
using json = nlohmann::json;

/**
 * This class is responsible for providing the tools to interface with the Verboze server
 */
class VerbozeAPI {
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
    static void SendCommand();
};
