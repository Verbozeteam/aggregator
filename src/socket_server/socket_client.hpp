#pragma once

#include <vector>
#include <string>

#include "json.hpp"
using json = nlohmann::json;

class SocketClient {
    /**
     * Internal use in harmony with the SocketServer to perform IO to the socket
     */
    friend class SocketServer;

    int m_client_fd;
    std::vector<unsigned char> m_write_buffer;
    std::vector<unsigned char> m_read_buffer;

    bool OnReadingAvailable();
    bool OnWritingAvailable();

protected:
    std::string m_client_ip;

public:
    SocketClient(int fd, std::string ip);
    ~SocketClient();

    void Write(json msg);
    bool OnMessage(json msg);
};

