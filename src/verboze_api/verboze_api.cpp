#include "config/config.hpp"
#include "logging/logging.hpp"
#include "verboze_api/verboze_api.hpp"

std::string VerbozeAPI::m_connection_token = "";
struct lws_context* VerbozeAPI::m_lws_context = nullptr;
std::thread VerbozeAPI::m_lws_thread;
bool VerbozeAPI::m_stop_thread = false;

static const struct lws_protocols g_protocols[] = {
	{
		"lws-broker",
		websocket_callback_broker,
		0,
		0,
	},
    {
		"http-broker",
		http_callback_broker,
        0,
		0,
	},
	{ NULL, NULL, 0, 0 }
};

struct lws_context* VerbozeAPI::GetLWSContext() {
    return m_lws_context;
}

void VerbozeAPI::__lws_thread() {
    int n = 0;
	while (!m_stop_thread && n >= 0) {
		__updateHTTP();
		n = lws_service(m_lws_context, 1000);
	}
}

int VerbozeAPI::Initialize() {
    m_connection_token = ConfigManager::get<std::string>("verboze-token");

	lws_set_log_level(0, NULL);

	struct lws_context_creation_info info;
	memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
	info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	info.protocols = g_protocols;
	info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

	VerbozeAPI::m_lws_context = lws_create_context(&info);
	if (!VerbozeAPI::m_lws_context) {
        LOG(error) << "Failed to initialize websockets";
		return 1;
	}

    m_stop_thread = false;
    m_lws_thread = std::thread(__lws_thread);

    LOG(info) << "Verboze API initialized";
    return 0;
}

void VerbozeAPI::Cleanup() {
    m_stop_thread = true;
    m_lws_thread.join();

	lws_context_destroy(VerbozeAPI::m_lws_context);
}
