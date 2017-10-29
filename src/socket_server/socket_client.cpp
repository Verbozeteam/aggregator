#include "logging/logging.hpp"
#include "socket_client.hpp"

#include <sys/socket.h>
#include <unistd.h>

SocketClient::SocketClient(int fd, std::string ip) : m_client_fd(fd), m_client_ip(ip) {
}

SocketClient::~SocketClient() {
    if (m_client_fd > 0)
        close(m_client_fd);
    m_client_fd = -1;
}

bool SocketClient::OnReadingAvailable() {
    char tmp_buf[4096];
    int rbytes = recv(m_client_fd, tmp_buf, 4096, 0);
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
    int wbytes = send(m_client_fd, &m_write_buffer[0], m_write_buffer.size(), 0);
    if (wbytes <= 0) {
        LOG(warning) << "Failed to write to client " << m_client_ip << " (fd " << m_client_fd << ")";
        return false;
    } else
        m_write_buffer.erase(m_write_buffer.begin(), m_write_buffer.begin() + wbytes);

    return true;
}

void SocketClient::Write(json msg) {
    std::string dump = msg.dump();
    const char* cstr = dump.c_str();
    size_t payload_size = strlen(cstr);
    m_write_buffer.push_back((unsigned char)((payload_size      ) & 0xFF));
    m_write_buffer.push_back((unsigned char)((payload_size >> 8 ) & 0xFF));
    m_write_buffer.push_back((unsigned char)((payload_size >> 16) & 0xFF));
    m_write_buffer.push_back((unsigned char)((payload_size >> 24) & 0xFF));
    m_write_buffer.insert(std::end(m_write_buffer), &cstr[0], &cstr[payload_size]);
}

bool SocketClient::OnMessage(json msg) {
    LOG(trace) << "Received message from " << m_client_ip << ": " << msg;
    return true;
}
