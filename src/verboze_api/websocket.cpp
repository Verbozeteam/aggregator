#include "config/config.hpp"
#include "logging/logging.hpp"
#include "verboze_api/verboze_api.hpp"

#include <libwebsockets.h>
#include <sys/time.h>

#include <thread>
#include <queue>

namespace ws_global {
    /** URL that the websocket will try to connect to */
    std::string g_connection_url = "";
    /** Callback to be called when a message arrives from the websocket */
    CommandCallback g_command_callback = nullptr;

    /** Thread running pump loop for lws */
    std::thread g_ws_thread;
    /** Flag to stop the g_ws_thread */
    bool g_stop_thread = false;
    /** Context used for the libwebsockets library */
	struct lws_context* g_context = nullptr;
    /** client pointer */
    struct lws* g_client_wsi;

    /** mutex to protext g_is_connected and data queue */
    std::mutex g_connection_mutex;
    /** whether connection is established */
    bool g_is_connected = false;
    /** queue of to-be-sent websocket messages */
    std::queue<std::string> g_message_queue;
};

static int connect_client() {
    int port = 80;
    bool is_ssl = false;
    std::string path = "";
    std::string address = "";

    std::string url = ws_global::g_connection_url;
    if (url.find("ws://") == 0) {
        url = url.substr(5);
    } else if (url.find("wss://") == 0) {
        url = url.substr(6);
        is_ssl = true;
        port = 443;
    }
    int slash_index = url.find("/");
    if (slash_index != std::string::npos) {
        path = url.substr(slash_index);
        url = url.substr(0, slash_index);
    }
    int colon_index = url.find(":");
    if (colon_index != std::string::npos) {
        try {
            std::string port_str = url.substr(colon_index+1);
            port = std::stoi(port_str);
        } catch (...) {
            LOG(error) << "Failed to read port in URL " << ws_global::g_connection_url;
        }
        url = url.substr(0, colon_index);
    }

    address = url;

    LOG(info) << "Websocket connecting to " << (is_ssl ? "wss://" : "ws://") << address << ":" << port << path;

	struct lws_client_connect_info info;
    memset(&info, 0, sizeof(struct lws_client_connect_info));
	info.context = ws_global::g_context;
	info.port = port;
	info.address = address.c_str();
	info.path = path.c_str();
	info.host = info.address;
	info.origin = info.address;
	info.ssl_connection = !is_ssl ? 0 :
        LCCSCF_USE_SSL |
        LCCSCF_ALLOW_SELFSIGNED |
        LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;

	info.protocol = "lws-broker";
	info.pwsi = &ws_global::g_client_wsi;

    return !lws_client_connect_via_info(&info);
}

static int callback_broker(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
        if (connect_client())
    		lws_timed_callback_vh_protocol(lws_get_vhost(wsi), lws_get_protocol(wsi), LWS_CALLBACK_USER, 1);
        break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
        ws_global::g_stop_thread = true;
        break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		LOG(error) << "WEBSOCKET CLIENT_CONNECTION_ERROR: " << (in ? (char *)in : "(null)");

	case LWS_CALLBACK_CLIENT_CLOSED:
		LOG(info) << "Websocket client closed";
        ws_global::g_connection_mutex.lock();
        ws_global::g_is_connected = false;
        ws_global::g_client_wsi = nullptr;
        ws_global::g_connection_mutex.unlock();
        // reconnect
		lws_timed_callback_vh_protocol(lws_get_vhost(wsi), lws_get_protocol(wsi), LWS_CALLBACK_USER, 1);
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
        ws_global::g_connection_mutex.lock();
        ws_global::g_is_connected = true;
        if (ws_global::g_message_queue.size())
            lws_callback_on_writable(ws_global::g_client_wsi);
		LOG(info) << "Websocket connected! (" << ws_global::g_message_queue.size() << " messages queued)";
        ws_global::g_connection_mutex.unlock();
		break;

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        std::string smsg;
        json jmsg;
        try {
            smsg = std::string((char*)in, len);
            jmsg = json::parse(smsg);
        } catch (...) {}

        if (!jmsg.is_null()) {
            LOG(trace) << "Got command from websocket: " << jmsg;
            if (ws_global::g_command_callback)
                ws_global::g_command_callback(jmsg);
        } else
            LOG(error) << "Got invalid JSON from websocket: " << smsg;
        
        break;
    }

	case LWS_CALLBACK_CLIENT_WRITEABLE: {
        ws_global::g_connection_mutex.lock();

        std::string msg = ws_global::g_message_queue.front();
        ws_global::g_message_queue.pop();
        unsigned char* cstr = new unsigned char[msg.size() + LWS_PRE];
        if (cstr) {
            memcpy(cstr + LWS_PRE, msg.c_str(), msg.size());
            int m = lws_write(wsi, cstr + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
            delete[] cstr;
            if (m < msg.size()) {
                ws_global::g_connection_mutex.unlock();
                LOG(error) << "WEBSOCKET ERROR: " << m << " writing to ws socket";
                return -1;
            } else
                LOG(trace) << "Sent to websocket: " << msg;
        } else
            LOG(error) << "WEBSOCKET ERROR: OUT OF MEMORY";

        if (ws_global::g_message_queue.size())
            lws_callback_on_writable(ws_global::g_client_wsi);

        ws_global::g_connection_mutex.unlock();

		break;
    }

	case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
		// Called when a message is added to ring buffer.
		if (ws_global::g_is_connected && !ws_global::g_stop_thread)
			lws_callback_on_writable(ws_global::g_client_wsi);
		break;

	case LWS_CALLBACK_USER:
		if (connect_client())
		    lws_timed_callback_vh_protocol(lws_get_vhost(wsi), lws_get_protocol(wsi), LWS_CALLBACK_USER, 1);
        break;

    default:
        break;
    }

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

void VerbozeAPI::__connect_websocket_async() {
    int n = 0;
	while (!ws_global::g_stop_thread && n >= 0)
		n = lws_service(ws_global::g_context, 1000);
}

void VerbozeAPI::SendCommand(json command) {
    ws_global::g_connection_mutex.lock();
    try {
        ws_global::g_message_queue.push(command.dump());
        if (ws_global::g_is_connected)
            lws_cancel_service(ws_global::g_context);
    } catch (...) {
        LOG(fatal) << "Failed to send websocket message";
    }
    ws_global::g_connection_mutex.unlock();
}


static const struct lws_protocols g_protocols[] = {
	{
		"lws-broker",
		callback_broker,
		0,
		0,
	},
	{ NULL, NULL, 0, 0 }
};
int VerbozeAPI::__initializeWebsockets() {
	lws_set_log_level(0, NULL);

    ws_global::g_connection_url = ConfigManager::get<std::string>("websocket-url");

	struct lws_context_creation_info info;

	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = g_protocols;
	info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	ws_global::g_context = lws_create_context(&info);
	if (!ws_global::g_context) {
        LOG(error) << "Failed to initialize websockets";
		return 1;
	}

    ws_global::g_stop_thread = false;
    ws_global::g_ws_thread = std::thread(__connect_websocket_async);

    return 0;
}

void VerbozeAPI::__cleanupWebsockets() {
    ws_global::g_stop_thread = true;
    ws_global::g_ws_thread.join();
	lws_context_destroy(ws_global::g_context);
}

void VerbozeAPI::SetCommandCallback(CommandCallback callback) {
    ws_global::g_command_callback = callback;
}
