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
        if (!OnRead()) {
            LOG(warning) << "Client " << m_client_ip << " (fd " << m_client_fd << ") communication failure";
            return false;
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

}

bool SocketClient::OnRead() {
    return true;
}
