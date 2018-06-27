#pragma once

#include <string>
#include <mutex>
#include <thread>
#include <queue>
#include <sys/time.h>

#include <libwebsockets.h>

#include <json.hpp>
using json = nlohmann::json;

typedef void (*CommandCallback) (json);
typedef void (*HttpResponseCallback) (class VerbozeHttpResponse);

/**
 * Represents a response from a verboze API call
 */
class VerbozeHttpResponse {
public:
    VerbozeHttpResponse() : status_code(200) {}
    VerbozeHttpResponse(int st, json d) : status_code(st), data(d) {}

    /** HTTP status code returned */
    int status_code;

    /** response json data */
    json data;

    /** response raw text/bytes */
    std::vector<char> raw_data;
};

/**
 * This class is responsible for providing the tools to interface with the Verboze server
 */
class VerbozeAPI {
    friend int websocket_callback_broker(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
    friend int http_callback_broker(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

    /** Connection token for this aggregator */
    static std::string m_connection_token;
    /** Context used for the libwebsockets library */
	static struct lws_context* m_lws_context;
    /** Thread running pump loop for lws */
    static std::thread m_lws_thread;
    /** Flag to stop the g_ws_thread */
    static bool m_stop_thread;

    /**
     * Entry point for a thread that will do lws services (connection, writing, reading, etc...)
     */
    static void __lws_thread();

public:
    /** Retrieves the LWS context */
    static struct lws_context* GetLWSContext();

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

    struct Endpoints {
        static void RegisterRoom(std::string room_id, HttpResponseCallback callback = nullptr);
    };
};

/** Websocket LWS callback broker */
int websocket_callback_broker(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len);
/** HTTP LWS callback broker */
int http_callback_broker(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
