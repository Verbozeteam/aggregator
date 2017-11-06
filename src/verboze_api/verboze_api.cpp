#include "config/config.hpp"
#include "logging/logging.hpp"
#include "verboze_api/verboze_api.hpp"

#include <sys/time.h>

#include <thread>

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

using namespace utility;
using namespace web::http;
using namespace web::http::client;
using namespace concurrency::streams;

std::string VerbozeAPI::m_connection_url = "";
websocket_callback_client VerbozeAPI::m_permenanat_client;
CommandCallback VerbozeAPI::m_command_callback = nullptr;
bool VerbozeAPI::m_websocket_connected = false;

void VerbozeAPI::__first_connection_thread() {
    m_permenanat_client = websocket_callback_client();

    while (true) {
        LOG(info) << "Attempting to connect to websocket on " << m_connection_url << "...";
        if (__reconnect() == 0) {
            try {
                // set receive handler
                m_permenanat_client.set_message_handler([](websocket_incoming_message msg) {
                    std::string smsg = msg.extract_string().get();
                    json jmsg;
                    try {
                        jmsg = json::parse(smsg);
                    } catch (...) {}
                    if (!jmsg.is_null()) {
                        LOG(trace) << "Got command from websocket: " << jmsg;
                        if (m_command_callback)
                            m_command_callback(jmsg);
                    } else
                        LOG(error) << "Got invalid JSON from websocket: " << smsg;
                });

                // set close handler
                m_permenanat_client.set_close_handler([](websocket_close_status close_status, const utility::string_t &reason, const std::error_code &error) {
                    LOG(warning) << "Websocket connection dropped: [status=" << (int)close_status << "] reason: " << reason << " (code " << error << ")";
                    sleep(5);
                    VerbozeAPI::__reconnect();
                });

                break;
            } catch (...) {
                LOG(error) << "Failed to set websocket handlers";
                m_permenanat_client = websocket_callback_client();
            }
        } else
            m_permenanat_client = websocket_callback_client();

        sleep(5);
    }

    LOG(info) << "Websocket connected to " << m_connection_url;
    m_websocket_connected = true;
}

int VerbozeAPI::__reconnect() {
    try {
        m_permenanat_client.connect(U(m_connection_url)).wait();
    } catch (...) {
        return -1;
    }

    return 0;
}

int VerbozeAPI::Initialize() {
    // http_client client(U("http://www.rimads.io/api-v1/countries/"));
    // client.request(methods::GET).then([=](http_response response) -> void {
    //     cout << "Response status code " << response.status_code() << std::endl;
    //     json j = json::parse(response.extract_string().get());
    //     cout << j << "\n";
    // }).wait();

    // return 0;

    m_connection_url = ConfigManager::get<std::string>("websocket-url");

    std::thread t(__first_connection_thread);
    t.detach();

    LOG(info) << "Verboze API initialized";

    return 0;
}

void VerbozeAPI::Cleanup() {
    try {
        if (m_websocket_connected) {
            m_permenanat_client.set_close_handler(nullptr);
            m_permenanat_client.close().wait();
        }
    } catch (...) {
        LOG(error) << "Failed to close connection to websocket";
    }
}

void VerbozeAPI::SendCommand(json command) {
    if (!m_websocket_connected)
        return; // don't do anything if cnnection is not up

    try {
        websocket_outgoing_message msg;
        msg.set_utf8_message(command.dump());
        m_permenanat_client.send(msg).wait(); // NEED NU2 WAIT!
    } catch (...) {
        LOG(fatal) << "Failed to send websocket message";
    }
}

void VerbozeAPI::SetCommandCallback(CommandCallback callback) {
    m_command_callback = callback;
}
