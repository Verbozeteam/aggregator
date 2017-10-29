#include "config/config.hpp"
#include "logging/logging.hpp"
#include "socket_server/socket_server.hpp"

#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <vector>

int SocketServer::m_hosting_port = 0;
std::unordered_map<int, SocketClient*> SocketServer::m_clients;
std::thread SocketServer::m_server_thread;

std::string read_ip(struct sockaddr_in address) {
    struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&address;
    struct in_addr ipAddr = pV4Addr->sin_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
    return std::string(str);
}

int SocketServer::__openHostingSocket(int max_connections) {
    struct sockaddr_in address;
    int opt = 1;

    int m_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (!m_socketfd) {
        LOG(error) << "Failed to create hosting socket";
        return -1;
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(m_socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        LOG(error) << "Failed to set hosting socket options";
        return -2;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_hosting_port);

    // Forcefully attaching socket to the port 8080
    if (bind(m_socketfd, (struct sockaddr *)&address, sizeof(address))<0) {
        LOG(error) << "Failed to bind hosting socket";
        return -3;
    }

    if (listen(m_socketfd, max_connections) < 0) {
        LOG(error) << "Failed to listen to hosting socket";
        return -4;
    }

    return m_socketfd;
}

void SocketServer::__thread_entry(int max_connections) {
    int server_fd = __openHostingSocket(max_connections);
    if (server_fd <= 0)
        return;

    LOG(info) << "Hosting socket server at 0.0.0.0:" << m_hosting_port;

    while (true) {
        fd_set read_fds, write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(server_fd, &read_fds);

        int maxfd = server_fd;
        for (auto it = m_clients.begin(); it != m_clients.end(); it++) {
            FD_SET(it->second->m_client_fd, &read_fds);
            if (it->second->m_write_buffer.size() > 0)
                FD_SET(it->second->m_client_fd, &write_fds);
            maxfd = std::max(maxfd, it->second->m_client_fd);
        }

        int ret = select(maxfd + 1, &read_fds, &write_fds, NULL, NULL);
        if (ret > 0) {
            if (FD_ISSET(server_fd, &read_fds)) {
                struct sockaddr_in address;
                int addrlen;
                int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                if (client_fd > 0) {
                    LOG(info) << "Socket client connectd";
                    RegisterClient(new SocketClient(client_fd, read_ip(address)));
                } else
                    LOG(error) << "Failed to accept connection";
            }

            std::vector<SocketClient*> clients_to_remove;
            for (auto it = m_clients.begin(); it != m_clients.end(); it++) {
                int clfd = it->second->m_client_fd;
                if (FD_ISSET(clfd, &read_fds)) {
                    // Reading is available
                    if (!it->second->OnReadingAvailable())
                        clients_to_remove.push_back(it->second);
                }
                if (FD_ISSET(clfd, &write_fds)) {
                    // Writing is available
                    if (!it->second->OnWritingAvailable())
                        clients_to_remove.push_back(it->second);
                }
            }

            for (auto it = clients_to_remove.begin(); it != clients_to_remove.end(); it++)
                DeregisterClient(*it);
        }
    }
}

void SocketServer::RegisterClient(SocketClient* client) {
    LOG(info) << "Registering client " << client->m_client_ip << " (fd " << client->m_client_fd << ")";
    m_clients.insert(std::pair<int, SocketClient*>(client->m_client_fd, client));
}

void SocketServer::DeregisterClient(SocketClient* client) {
    LOG(info) << "Deregistering client " << client->m_client_ip << " (fd " << client->m_client_fd << ")";
    if (m_clients.find(client->m_client_fd) != m_clients.end())
        m_clients.erase(client->m_client_fd);
}

int SocketServer::Initialize() {
    m_hosting_port = ConfigManager::get<int>("hosting-port");
    int max_clients = ConfigManager::get<int>("max-socket-clients");
    m_server_thread = std::thread(SocketServer::__thread_entry, max_clients + 1);
    return 0;
}

void SocketServer::WaitForCompletion() {
    m_server_thread.join();
}
