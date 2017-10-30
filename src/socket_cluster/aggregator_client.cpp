#include "logging/logging.hpp"
#include "socket_cluster/aggregator_client.hpp"

AggregatorClient::AggregatorClient(int fd, std::string ip) : SocketClient(fd, ip) {
}

void AggregatorClient::Write(json msg) {
    SocketClient::Write(msg); // actually writes to the buffer

    /** Perform caching */

    LOG(trace) << "AggregatorClient::Write()";
}

bool AggregatorClient::OnMessage(json msg) {
    SocketClient::OnMessage(msg);

    /** Perform caching */

    LOG(trace) << "AggregatorClient::OnMessage()";

    return true;
}
