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

int VerbozeAPI::Initialize() {
    // http_client client(U("http://www.rimads.io/api-v1/countries/"));
    // client.request(methods::GET).then([=](http_response response) -> void {
    //     cout << "Response status code " << response.status_code() << std::endl;
    //     json j = json::parse(response.extract_string().get());
    //     cout << j << "\n";
    // }).wait();

    // return 0;

    try {
        m_permenanat_client.connect(U("ws://localhost:1234")).wait();
    } catch (...) {
        LOG(fatal) << "Failed to connect websocket to server";
        return -1;
    }

    try {
        // set receive handler
        m_permenanat_client.set_message_handler([](websocket_incoming_message msg) {
            std::string s = msg.extract_string().get();
            LOG(trace) << "Got message: " << s;
        });
    } catch (...) {
        LOG(fatal) << "Failed to set message handler";
        return -2;
    }

    try {
        websocket_outgoing_message msg;
        msg.set_utf8_message("I am a UTF-8 string! (Or close enough...)");
        m_permenanat_client.send(msg); // NEED NU2 WAIT!
    } catch (...) {
        LOG(fatal) << "Failed to send message";
        return -3;
    }

    LOG(info) << "Verboze API initialized";

    while (1);

    return 0;
}

void VerbozeAPI::Cleanup() {

}

void VerbozeAPI::SendCommand() {

}
