#pragma once

#include <string>
#include <sys/socket.h>

/** Just a robust write() call */
int __robust_write(int fd, uint8_t* buf, size_t buflen);

/** Just a robust send() call */
int __robust_send(int fd, uint8_t* buf, size_t buflen, int flags=0);

/** Just a robust sendto() call */
int __robust_sendto(int fd, char* buf, int buflen, int flags, struct sockaddr* addr, socklen_t addr_size);

/** Just a robust read() call */
int __robust_read(int fd, uint8_t* buf, size_t maxread);

/** Just a robust recv() call */
int __robust_recv(int fd, uint8_t* buf, size_t maxread, int flags=0);

/** Just a robust recvfrom() call */
int __robust_recvfrom(int fd, uint8_t* buf, size_t maxread, int flags, struct sockaddr* src_addr, socklen_t* addrlen);

/** reads an IPv4 address (x.x.x.x) out of a sockaddr_in */
std::string __read_ip(struct sockaddr_in address);

