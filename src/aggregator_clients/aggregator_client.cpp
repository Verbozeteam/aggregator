#include "logging/logging.hpp"
#include "aggregator_clients/aggregator_client.hpp"

AggregatorClient::AggregatorClient(int fd, std::string ip) : SocketClient(fd, ip) {
}

void AggregatorClient::Write(json msg) {
    SocketClient::Write(msg); // actually writes to the buffer

    /** Perform caching */
}

bool AggregatorClient::OnMessage(json msg) {
    SocketClient::OnMessage(msg);

    /** Perform caching */

    return true;
}
