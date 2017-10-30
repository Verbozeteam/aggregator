#include <config/config.hpp>
#include <logging/logging.hpp>
#include <logging/logging.hpp>
#include <socket_cluster/socket_cluster.hpp>
#include <socket_cluster/aggregator_client.hpp>

#include <time.h>

int main(int argc, char* argv[]) {
    if (ConfigManager::LoadFromCommandline(argc, argv) != 0)
        return 1;

    if (Log::Initialize() != 0)
        return 2;

    SocketCluster::Initialize();

    SocketClientPtr sc = SocketClient::Create<AggregatorClient> ("10.11.28.41", 4567);
    if (sc)
        sc->Write("{\"code\": 0}"_json);
    sleep(2);
    SocketCluster::DeregisterClient(sc);
    sc.reset();

    SocketCluster::Kill();

    SocketCluster::WaitForCompletion();

    SocketCluster::Cleanup();

    Log::Cleanup();

    return 0;
}
