#include "config/config.hpp"
#include "logging/logging.hpp"
#include "verboze_api/verboze_api.hpp"

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

using namespace utility;
using namespace web::http;
using namespace web::http::client;
using namespace concurrency::streams;

websocket_callback_client VerbozeAPI::m_permenanat_client;
CommandCallback VerbozeAPI::m_command_callback = nullptr;

int VerbozeAPI::Initialize() {
    // http_client client(U("http://www.rimads.io/api-v1/countries/"));
    // client.request(methods::GET).then([=](http_response response) -> void {
    //     cout << "Response status code " << response.status_code() << std::endl;
    //     json j = json::parse(response.extract_string().get());
    //     cout << j << "\n";
    // }).wait();

    // return 0;

    try {
        std::string url = ConfigManager::get<std::string>("websocket-url");
        m_permenanat_client.connect(U(url)).wait();
    } catch (...) {
        LOG(fatal) << "Failed to connect websocket to server";
        return -1;
    }

    try {
        // set receive handler
        m_permenanat_client.set_message_handler([](websocket_incoming_message msg) {
            std::string smsg = msg.extract_string().get();
            json jmsg = json::parse(smsg);
            if (!jmsg.is_null()) {
                LOG(trace) << "Got command: " << jmsg;
                if (m_command_callback)
                    m_command_callback(jmsg);
            } else
                LOG(error) << "Got invalid JSON from websocket: " << smsg;
        });
    } catch (...) {
        LOG(fatal) << "Failed to set message handler";
        return -2;
    }

    LOG(info) << "Verboze API initialized";

    return 0;
}

void VerbozeAPI::Cleanup() {
    try {
        m_permenanat_client.close().wait();
    } catch (...) {
        LOG(error) << "Failed to close connection to websocket";
    }
}

void VerbozeAPI::SendCommand(json command) {
    try {
        websocket_outgoing_message msg;
        msg.set_utf8_message(command.dump());
        m_permenanat_client.send(msg); // NEED NU2 WAIT!
    } catch (...) {
        LOG(fatal) << "Failed to send message";
    }
}

void VerbozeAPI::SetCommandCallback(CommandCallback callback) {
    m_command_callback = callback;
}