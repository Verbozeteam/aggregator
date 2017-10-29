#pragma once

#include <vector>
#include <string>

#include "json.hpp"
using json = nlohmann::json;

class SocketClient {
    friend class SocketServer;

    int m_client_fd;
    std::string m_client_ip;
    std::vector<unsigned char> m_write_buffer;
    std::vector<unsigned char> m_read_buffer;

    bool OnReadingAvailable();
    bool OnWritingAvailable();

public:
    SocketClient(int fd, std::string ip);
    ~SocketClient();

    void Write(json msg);
    bool OnRead();
};

