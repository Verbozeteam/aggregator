#include "config/config.hpp"
#include "logging/logging.hpp"
#include "socket_cluster/socket_cluster.hpp"

#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <vector>


int __robust_write(int fd, uint8_t* buf, size_t buflen) {
    int attempt = 0;
    int wbytes;
    while (attempt++ < 5) {
        wbytes = write(fd, buf, buflen);
        if (wbytes < 0 && (errno == EINTR || errno == EAGAIN))
            continue;
        break;
    }
    return wbytes;
}

int __robust_send(int fd, uint8_t* buf, size_t buflen, int flags) {
    int attempt = 0;
    int wbytes;
    while (attempt++ < 5) {
        wbytes = send(fd, buf, buflen, flags);
        if (wbytes < 0 && (errno == EINTR || errno == EAGAIN))
            continue;
        break;
    }
    return wbytes;
}

int __robust_read(int fd, uint8_t* buf, size_t maxread) {
    int attempt = 0;
    int rbytes;
    while (attempt++ < 5) {
        rbytes = read(fd, buf, maxread);
        if (rbytes < 0 && (errno == EINTR || errno == EAGAIN))
            continue;
        break;
    }
    return rbytes;
}

int __robust_recv(int fd, uint8_t* buf, size_t maxread, int flags) {
    int attempt = 0;
    int rbytes;
    while (attempt++ < 5) {
        rbytes = recv(fd, buf, maxread, flags);
        if (rbytes < 0 && (errno == EINTR || errno == EAGAIN))
            continue;
        break;
    }
    return rbytes;
}

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
        return sockfd;
    }

    return sockfd;
}

SocketClient::SocketClient(int fd, std::string ip) : m_client_fd(fd), m_client_ip(ip) {
    LOG(trace) << "SocketClient() c'tor";
}

SocketClient::~SocketClient() {
    LOG(trace) << "~SocketClient() d'tor";
    if (m_client_fd > 0)
        close(m_client_fd);
    m_client_fd = -1;
}

bool SocketClient::OnReadingAvailable() {
    LOG(trace) << "OnReadingAvailable()";
    uint8_t tmp_buf[4096];
    int rbytes = __robust_recv(m_client_fd, tmp_buf, 4096);
    if (rbytes <= 0) {
        LOG(info) << "Client " << m_client_ip << " (fd " << m_client_fd << ") closed the cnnection";
        return false;
    } else {
        m_read_buffer.insert(std::end(m_read_buffer), &tmp_buf[0], &tmp_buf[rbytes]);
        if (m_read_buffer.size() >= 4) {
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
                    LOG(warning) << "Client " << m_client_ip << " sent invalid JSON " << s;
                }
                delete[] s;
                if (j.is_null() || j.size() == 0 || !OnMessage(j)) {
                    LOG(warning) << "Client " << m_client_ip << " (fd " << m_client_fd << ") communication failure";
                    return false;
                }
            }
        }
    }

    return true;
}

bool SocketClient::OnWritingAvailable() {
    LOG(trace) << "OnWritingAvailable()";
    m_write_buffer_mutex.lock();
    std::vector<uint8_t> staging_buffer (m_write_buffer);
    m_write_buffer_mutex.unlock();
    int wbytes = __robust_send(m_client_fd, &staging_buffer[0], staging_buffer.size());
    if (wbytes <= 0) {
        LOG(warning) << "Failed to write to client " << m_client_ip << " (fd " << m_client_fd << ")";
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
    m_write_buffer.insert(std::end(m_write_buffer), &cstr[0], &cstr[payload_size]);
    m_write_buffer_mutex.unlock();

    LOG(trace) << "Sent message from " << m_client_ip << ": " << msg;
}

bool SocketClient::OnMessage(json msg) {
    LOG(trace) << "Received message from " << m_client_ip << ": " << msg;
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
std::unordered_map<std::string, SocketClientPtr> SocketCluster::m_clients_by_ip;
std::thread SocketCluster::m_server_thread;

std::string read_ip(struct sockaddr_in address) {
    struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&address;
    struct in_addr ipAddr = pV4Addr->sin_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
    return std::string(str);
}

void SocketCluster::__thread_entry(int max_connections) {
    while (m_is_alive) {
        fd_set read_fds, write_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        FD_SET(m_event_pipe_read_end, &read_fds);

        int maxfd = m_event_pipe_read_end;

        std::vector<SocketClientPtr> clients;
        m_clients_mutex.lock_shared(); // read lock
        for (auto it = m_clients.begin(); it != m_clients.end(); it++)
            clients.push_back(it->second);
        m_clients_mutex.unlock_shared();

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

    LOG(info) << "SocketCluster thread exiting";
}

void SocketCluster::RegisterClient(SocketClientPtr client) {
    m_clients_mutex.lock(); // write (exclusive) lock
    LOG(info) << "Registering client " << client->m_client_ip << " (fd " << client->m_client_fd << ")";
    m_clients.insert(std::pair<int, SocketClientPtr>(client->m_client_fd, client));
    m_clients_by_ip.insert(std::pair<std::string, SocketClientPtr>(client->m_client_ip, client));
    m_clients_mutex.unlock();

    Notify();
}

void SocketCluster::DeregisterClient(SocketClientPtr client) {
    m_clients_mutex.lock(); // write (exclusive) lock
    LOG(info) << "Deregistering client " << client->m_client_ip << " (fd " << client->m_client_fd << ")";
    if (m_clients.find(client->m_client_fd) != m_clients.end()) {
        m_clients.erase(client->m_client_fd);
        m_clients_by_ip.erase(client->m_client_ip);
    }
    m_clients_mutex.unlock();

    Notify();
}

int SocketCluster::Initialize() {
    int max_clients = ConfigManager::get<int>("max-socket-clients");
    m_is_alive = true;

    int pipe_ends[2];
    if (pipe(pipe_ends) != 0)
        return -1;
    m_event_pipe_read_end = pipe_ends[0];
    m_event_pipe_write_end = pipe_ends[1];

    m_server_thread = std::thread(SocketCluster::__thread_entry, max_clients + 1);

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
    m_clients_by_ip.clear();
    m_clients_mutex.unlock();

    if (m_event_pipe_read_end > 0)
        close(m_event_pipe_read_end);
    if (m_event_pipe_write_end > 0)
        close(m_event_pipe_write_end);

    m_event_pipe_read_end = m_event_pipe_write_end = -1;
}

void SocketCluster::Notify() {
    uint8_t buf[1] = {0};
    __robust_write(m_event_pipe_write_end, buf, 1);
}

void SocketCluster::Kill() {
    m_is_alive = false;
    Notify();
}
