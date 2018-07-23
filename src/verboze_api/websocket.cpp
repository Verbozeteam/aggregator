#include "config/config.hpp"
#include "logging/logging.hpp"
#include "verboze_api/verboze_api.hpp"

namespace ws_global {
    /** Callback to be called when a message arrives from the websocket */
    CommandCallback g_command_callback = nullptr;
    /** mutex to protext g_is_connected and data queue */
    std::mutex g_connection_mutex;
    /** whether connection is established */
    bool g_is_connected = false;
    /** queue of to-be-sent websocket messages */
    std::queue<std::string> g_message_queue;

    /** lws client */
    struct lws* g_client_wsi = nullptr;
};

bool VerbozeAPI::IsWebsocketConnected() {
    return ws_global::g_is_connected;
}

std::string VerbozeAPI::TokenToStreamURL(std::string token) {
    std::string url = ConfigManager::get<std::string>("verboze-url");
    std::string protocol = ConfigManager::get<std::string>("ws-protocol");
    if (url.size() > 0) {
        url = protocol + "://" + url;
        if (url.at(url.size()-1) == '/')
            url = url.substr(0, url.size() -1);
    }
    url += "/stream/" + token;
    return url;
}

static int connect_ws_client(std::string token) {
    int port = 80;
    bool is_ssl = false;
    std::string path = "";
    std::string address = "";
    std::string url = VerbozeAPI::TokenToStreamURL(token);
    std::string new_url = url;

    if (new_url.find("ws://") == 0) {
        new_url = new_url.substr(5);
    } else if (new_url.find("wss://") == 0) {
        new_url = new_url.substr(6);
        is_ssl = true;
        port = 443;
    }
    int slash_index = (int)new_url.find("/");
    if (slash_index != (int)std::string::npos) {
        path = new_url.substr(slash_index);
        new_url = new_url.substr(0, slash_index);
    }
    int colon_index = (int)new_url.find(":");
    if (colon_index != (int)std::string::npos) {
        try {
            std::string port_str = new_url.substr(colon_index+1);
            port = std::stoi(port_str);
        } catch (...) {
            LOG(error) << "Failed to read port in URL " << url;
        }
        new_url = new_url.substr(0, colon_index);
    }

    address = new_url;

    LOG(info) << "Websocket connecting to " << (is_ssl ? "wss://" : "ws://") << address << ":" << port << path;

	struct lws_client_connect_info info;
    memset(&info, 0, sizeof(struct lws_client_connect_info));
	info.context = VerbozeAPI::GetLWSContext();
	info.port = port;
	info.address = address.c_str();
	info.path = path.c_str();
	info.host = info.address;
	info.origin = info.address;
	info.ssl_connection = !is_ssl ? 0 :
        LCCSCF_USE_SSL |
        LCCSCF_ALLOW_SELFSIGNED |
        LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
	info.pwsi = &ws_global::g_client_wsi;

	info.protocol = "lws-broker";

    return !lws_client_connect_via_info(&info);
}

int websocket_callback_broker(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
        if (connect_ws_client(VerbozeAPI::m_connection_token) != 0)
    		lws_timed_callback_vh_protocol(lws_get_vhost(wsi), lws_get_protocol(wsi), LWS_CALLBACK_USER, 3);
        break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
        VerbozeAPI::m_stop_thread = true;
        break;

	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		LOG(error) << "WEBSOCKET CLIENT_CONNECTION_ERROR: " << (in ? (char *)in : "(null)");

	case LWS_CALLBACK_CLIENT_CLOSED:
		LOG(info) << "Websocket client closed";
        ws_global::g_connection_mutex.lock();
        ws_global::g_is_connected = false;
        ws_global::g_connection_mutex.unlock();
        // reconnect
		lws_timed_callback_vh_protocol(lws_get_vhost(wsi), lws_get_protocol(wsi), LWS_CALLBACK_USER, 3);
		break;

	case LWS_CALLBACK_CLIENT_ESTABLISHED:
        ws_global::g_connection_mutex.lock();
        ws_global::g_is_connected = true;
        if (ws_global::g_message_queue.size())
            lws_callback_on_writable(wsi);
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

        if (ws_global::g_message_queue.size() > 0) {
            std::string msg = ws_global::g_message_queue.front();
            ws_global::g_message_queue.pop();
            unsigned char* cstr = new unsigned char[msg.size() + LWS_PRE];
            if (cstr) {
                memcpy(cstr + LWS_PRE, msg.c_str(), msg.size());
                int m = lws_write(wsi, cstr + LWS_PRE, msg.size(), LWS_WRITE_TEXT);
                delete[] cstr;
                if (m < (int)msg.size()) {
                    ws_global::g_connection_mutex.unlock();
                    LOG(error) << "WEBSOCKET ERROR: " << m << " writing to ws socket";
                    return -1;
                } else
                    LOG(trace) << "Sent to websocket: " << msg;
            } else
                LOG(error) << "WEBSOCKET ERROR: OUT OF MEMORY";

            if (ws_global::g_message_queue.size())
                lws_callback_on_writable(wsi);
        }

        ws_global::g_connection_mutex.unlock();

		break;
    }

	case LWS_CALLBACK_USER:
		if (connect_ws_client(VerbozeAPI::m_connection_token) != 0)
		    lws_timed_callback_vh_protocol(lws_get_vhost(wsi), lws_get_protocol(wsi), LWS_CALLBACK_USER, 3);
        break;

    default:
        break;
    }

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

void VerbozeAPI::SendCommand(json command) {
    ws_global::g_connection_mutex.lock();
    try {
        ws_global::g_message_queue.push(command.dump());
        if (ws_global::g_is_connected)
			lws_callback_on_writable(ws_global::g_client_wsi);
    } catch (...) {
        LOG(fatal) << "Failed to send websocket message";
    }
    ws_global::g_connection_mutex.unlock();
}

void VerbozeAPI::SetCommandCallback(CommandCallback callback) {
    ws_global::g_command_callback = callback;
}
