#include "utilities/network_utilities.hpp"

#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

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

int __robust_SSL_write(SSL* ssl, void* buf, int buflen) {
    int attempt = 0;
    int wbytes;
    while (attempt++ < 5) {
        wbytes = SSL_write(ssl, buf, buflen);
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

int __robust_sendto(int fd, char* buf, int buflen, int flags, struct sockaddr* addr, socklen_t addr_size) {
    int attempt = 0;
    int wbytes;
    while (attempt++ < 5) {
        wbytes = sendto(fd, buf, buflen, flags, addr, addr_size);
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

int __robust_SSL_read(SSL* ssl, void* buf, int maxread) {
    int attempt = 0;
    int rbytes;
    while (attempt++ < 5) {
        rbytes = SSL_read(ssl, buf, maxread);
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

int __robust_recvfrom(int fd, uint8_t* buf, size_t maxread, int flags, struct sockaddr* src_addr, socklen_t* addrlen) {
    int attempt = 0;
    int rbytes;
    while (attempt++ < 5) {
        rbytes = recvfrom(fd, buf, maxread, flags, src_addr, addrlen);
        if (rbytes < 0 && (errno == EINTR || errno == EAGAIN))
            continue;
        break;
    }
    return rbytes;
}

std::string __read_ip(struct sockaddr_in address) {
    struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&address;
    struct in_addr ipAddr = pV4Addr->sin_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
    return std::string(str);
}
