#pragma once

class SocketClient {
    int m_client_fd;

public:
    SocketClient(int fd);
    ~SocketClient();

};

