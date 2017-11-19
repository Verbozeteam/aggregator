#include "config/config.hpp"
#include "logging/logging.hpp"
#include "socket_cluster/socket_cluster.hpp"
#include "utilities/network_utilities.hpp"

#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <vector>

/*******************************************************************************************
 * CLIENT
 *******************************************************************************************/

int SocketClient::__openConnection(std::string ip, int port) {
    /* Create a socket point */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        LOG(error) << "Failed to create socket FD to (" << ip << ", " << port << ")";
        return sockfd;
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    serv_addr.sin_port = htons(port);

    /* Now connect to the server */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG(error) << "Failed to connect to (" << ip << ", " << port << ")";
        close(sockfd);
        return -1;
    }

    return sockfd;
}

SocketClient::SocketClient(int fd, std::string ip, int port) : m_client_fd(fd), m_ip(ip), m_port(port), m_identifier(ip+":"+std::to_string(port)) {
}

SocketClient::~SocketClient() {
    if (m_client_fd > 0)
        close(m_client_fd);
    m_client_fd = -1;
}

bool SocketClient::OnReadingAvailable() {
    uint8_t tmp_buf[4096];
    int rbytes = __robust_recv(m_client_fd, tmp_buf, 4096);
    if (rbytes <= 0) {
        LOG(info) << "Client " << m_ip << " (fd " << m_client_fd << ") closed the cnnection";
        return false;
    } else {
        m_read_buffer.insert(std::end(m_read_buffer), tmp_buf, tmp_buf + rbytes);
        while (m_read_buffer.size() >= 4) {
            size_t payload_size =
                ((((size_t)m_read_buffer[0]) & 0xFF)      ) |
                ((((size_t)m_read_buffer[1]) & 0xFF) << 8 ) |
                ((((size_t)m_read_buffer[2]) & 0xFF) << 16) |
                ((((size_t)m_read_buffer[3]) & 0xFF) << 24);
            if (payload_size > 0xFFFFFF) // fuck off.
                return false;
            if (m_read_buffer.size() >= 4 + payload_size) {
                // create a buffer containing the JSON message
                char* s = new char[payload_size+1];
                memcpy(s, &m_read_buffer[4], payload_size);
                s[payload_size] = 0;
                m_read_buffer.erase(m_read_buffer.begin(), m_read_buffer.begin() + 4 + payload_size);
                json j;
                try {
                    j = json::parse(s);
                    if (j.is_null())
                        throw;
                } catch (...) {
                    LOG(warning) << "Client " << m_ip << " sent invalid JSON " << s;
                }
                delete[] s;
                if (j.is_null() || !OnMessage(j)) {
                    LOG(warning) << "Client " << m_ip << " (fd " << m_client_fd << ") communication failure";
                    return false;
                }
            } else
                break;
        }
    }

    return true;
}

bool SocketClient::OnWritingAvailable() {
    m_write_buffer_mutex.lock();
    std::vector<uint8_t> staging_buffer (m_write_buffer);
    m_write_buffer_mutex.unlock();
    int wbytes = __robust_send(m_client_fd, &staging_buffer[0], staging_buffer.size());
    if (wbytes <= 0) {
        LOG(warning) << "Failed to write to client " << m_ip << " (fd " << m_client_fd << ")";
        return false;
    } else {
        m_write_buffer_mutex.lock();
        m_write_buffer.erase(m_write_buffer.begin(), m_write_buffer.begin() + wbytes);
        m_write_buffer_mutex.unlock();
    }

    return true;
}

void SocketClient::Write(json msg) {
    std::string dump = msg.dump();
    const char* cstr = dump.c_str();
    size_t payload_size = strlen(cstr);
    m_write_buffer_mutex.lock();
    m_write_buffer.push_back((uint8_t)((payload_size      ) & 0xFF));
    m_write_buffer.push_back((uint8_t)((payload_size >> 8 ) & 0xFF));
    m_write_buffer.push_back((uint8_t)((payload_size >> 16) & 0xFF));
    m_write_buffer.push_back((uint8_t)((payload_size >> 24) & 0xFF));
    m_write_buffer.insert(std::end(m_write_buffer), cstr, cstr + payload_size);
    m_write_buffer_mutex.unlock();

    SocketCluster::Notify();

    LOG(trace) << "Sent message to " << m_ip << ": " << msg;
}

bool SocketClient::OnMessage(json msg) {
    LOG(trace) << "Received message from " << m_ip << ": " << msg;
    return true;
}

/*******************************************************************************************
 * CLUSTER
 *******************************************************************************************/

bool SocketCluster::m_is_alive = true;
int SocketCluster::m_event_pipe_read_end = -1;
int SocketCluster::m_event_pipe_write_end = -1;
std::shared_timed_mutex SocketCluster::m_clients_mutex;
std::unordered_map<int, SocketClientPtr> SocketCluster::m_clients;
std::unordered_map<std::string, SocketClientPtr> SocketCluster::m_clients_by_id;
std::thread SocketCluster::m_server_thread;

void SocketCluster::__thread_entry() {
    while (m_is_alive) {
        fd_set read_fds, write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        FD_SET(m_event_pipe_read_end, &read_fds);

        int maxfd = m_event_pipe_read_end;

        std::vector<SocketClientPtr> clients = GetClientsList();

        for (auto it = clients.begin(); it != clients.end(); it++) {
            SocketClientPtr cl = *it;
            FD_SET(cl->m_client_fd, &read_fds);
            if (cl->m_write_buffer.size() > 0)
                FD_SET(cl->m_client_fd, &write_fds);
            maxfd = std::max(maxfd, cl->m_client_fd);
        }

        int ret = select(maxfd + 1, &read_fds, &write_fds, NULL, NULL);
        if (ret > 0) {
            if (FD_ISSET(m_event_pipe_read_end, &read_fds)) {
                // clear all (128 is a magic number, put anything...)
                uint8_t tmp_buf[128];
                __robust_read(m_event_pipe_read_end, tmp_buf, 128);
            }

            for (auto it = clients.begin(); it != clients.end(); it++) {
                SocketClientPtr cl = *it;
                int clfd = cl->m_client_fd;
                if (FD_ISSET(clfd, &read_fds)) {
                    // Reading is available
                    if (!cl->OnReadingAvailable()) {
                        DeregisterClient(cl);
                        continue;
                    }
                }
                if (FD_ISSET(clfd, &write_fds)) {
                    // Writing is available
                    if (!cl->OnWritingAvailable()) {
                        DeregisterClient(cl);
                        continue;
                    }
                }
            }
        } else
            LOG(warning) << "Select failed: " << ret << " (errno=" << errno << ")";
    }

    LOG(info) << "SocketCluster thread shutting down...";
}

void SocketCluster::RegisterClient(SocketClientPtr client) {
    m_clients_mutex.lock(); // write (exclusive) lock
    LOG(info) << "Registering client " << client->m_ip << " (fd " << client->m_client_fd << ")";
    m_clients.insert(std::pair<int, SocketClientPtr>(client->m_client_fd, client));
    m_clients_by_id.insert(std::pair<std::string, SocketClientPtr>(client->m_identifier, client));
    m_clients_mutex.unlock();

    Notify();
}

void SocketCluster::DeregisterClient(SocketClientPtr client) {
    m_clients_mutex.lock(); // write (exclusive) lock
    LOG(info) << "Deregistering client " << client->m_ip << " (fd " << client->m_client_fd << ")";
    if (m_clients.find(client->m_client_fd) != m_clients.end()) {
        m_clients.erase(client->m_client_fd);
        m_clients_by_id.erase(client->m_identifier);
    }
    m_clients_mutex.unlock();

    Notify();
}

int SocketCluster::Initialize() {
    m_is_alive = true;

    int pipe_ends[2];
    if (pipe(pipe_ends) != 0)
        return -1;
    m_event_pipe_read_end = pipe_ends[0];
    m_event_pipe_write_end = pipe_ends[1];

    m_server_thread = std::thread(SocketCluster::__thread_entry);

    LOG(info) << "SocketCluster ready";

    return 0;
}

void SocketCluster::WaitForCompletion() {
    if (m_server_thread.joinable())
        m_server_thread.join();
}

void SocketCluster::Cleanup() {
    Kill();
    WaitForCompletion();

    m_clients_mutex.lock(); // write (exclusive) lock
    m_clients.clear();
    m_clients_by_id.clear();
    m_clients_mutex.unlock();

    if (m_event_pipe_read_end > 0)
        close(m_event_pipe_read_end);
    if (m_event_pipe_write_end > 0)
        close(m_event_pipe_write_end);

    m_event_pipe_read_end = m_event_pipe_write_end = -1;

    LOG(info) << "SocketCluster shut down";
}

void SocketCluster::Notify() {
    uint8_t buf[1] = {0};
    __robust_write(m_event_pipe_write_end, buf, 1);
}

void SocketCluster::Kill() {
    m_is_alive = false;
    Notify();
}

bool SocketCluster::IsClientRegistered(std::string id) {
    m_clients_mutex.lock_shared(); // read lock
    bool ret = m_clients_by_id.find(id) != m_clients_by_id.end();
    m_clients_mutex.unlock_shared();
    return ret;
}

SocketClientPtr SocketCluster::GetClient(std::string id) {
    m_clients_mutex.lock_shared(); // read lock
    auto it = m_clients_by_id.find(id);
    m_clients_mutex.unlock_shared();
    return it == m_clients_by_id.end() ? nullptr : it->second;
}

std::vector<SocketClientPtr> SocketCluster::GetClientsList() {
    std::vector<SocketClientPtr> clients;
    m_clients_mutex.lock_shared(); // read lock
    for (auto it = m_clients.begin(); it != m_clients.end(); it++)
        clients.push_back(it->second);
    m_clients_mutex.unlock_shared();
    return clients;
}
