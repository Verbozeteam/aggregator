#include "socket_server/socket_server.hpp"

SocketServer::SocketServer() {

}

SocketServer::~SocketServer() {

}

int SocketServer::RunThread(int max_connections) {

    return 0;
}

void SocketServer::__thread_entry(void* ptr) {
    SocketServer* server = (SocketServer*)ptr;

    while (true) {

    }
}