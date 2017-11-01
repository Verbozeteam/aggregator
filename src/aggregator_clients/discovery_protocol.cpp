#include "config/config.hpp"
#include "logging/logging.hpp"
#include "aggregator_clients/discovery_protocol.hpp"
#include "utilities/network_utilities.hpp"

#include <ifaddrs.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>

const uint8_t DISCOVERY_MAGIC[2] = {0x29, 0xad};

std::vector<std::string> DiscoveryProtocol::m_broadcast_interfaces;
std::unordered_map<std::string, DiscoveryProtocol::NetworkInterface> DiscoveryProtocol::m_interface_map;
DiscoveryCallback DiscoveryProtocol::m_callback;
std::thread DiscoveryProtocol::m_listener_thread;
std::mutex DiscoveryProtocol::m_interfaces_lock;
int DiscoveryProtocol::m_event_pipe_read_end = -1;
int DiscoveryProtocol::m_event_pipe_write_end = -1;

DiscoveryProtocol::NetworkInterface::NetworkInterface() : broadcast_socket(-1) {
}

int DiscoveryProtocol::NetworkInterface::create() {
    if ((broadcast_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        LOG(warning) << "Failed to create UDP socket for " << name;
        return -1;
    }

    int broadcastPermission = 1;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(int)) < 0) {
        LOG(warning) << "Failed to set broadcast permission for " << name;
    }

    int reuseAddr = 1;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_REUSEADDR, (void *) &reuseAddr, sizeof(int)) < 0) {
        LOG(warning) << "Failed to set address reuse for " << name;
    }

    broadcast_struct.sin_port = htons(BROADCAST_PORT);

    return 0;
}

void DiscoveryProtocol::NetworkInterface::destroy() {
    if (broadcast_socket > 0)
        close(broadcast_socket);

    broadcast_socket = -1;
}

void DiscoveryProtocol::NetworkInterface::send_broadcast() {
    uint8_t packet[] = {DISCOVERY_MAGIC[0], DISCOVERY_MAGIC[1], 0x0, 0x0};
    if (__robust_sendto(broadcast_socket, (char*)packet, sizeof(packet), 0, (struct sockaddr *)&broadcast_struct, sizeof(broadcast_struct)) != sizeof(packet))
        LOG(warning) << "Failed to send broadcast on interface " << name << " (fd " << broadcast_socket << ")";
}

DiscoveryProtocol::NetworkInterface DiscoveryProtocol::__parseIFA(struct ifaddrs* a) {
    DiscoveryProtocol::NetworkInterface iface;

    iface.name = a->ifa_name;
    iface.flags = a->ifa_flags;

    memcpy(&iface.addr_struct, a->ifa_addr, sizeof(struct sockaddr));
    memcpy(&iface.netmask_struct, a->ifa_netmask, sizeof(struct sockaddr));
    memcpy(&iface.broadcast_struct, a->ifa_broadaddr, sizeof(struct sockaddr));

    iface.address = __read_ip(iface.addr_struct);
    iface.netmask = __read_ip(iface.netmask_struct);
    iface.broadcast = __read_ip(iface.broadcast_struct);

    return iface;
}

bool DiscoveryProtocol::__isInterfaceAccepted(std::string name) {
    if (m_broadcast_interfaces.size() == 0)
        return true;
    for (int i = 0; i < m_broadcast_interfaces.size(); i++)
        if (m_broadcast_interfaces[i] == name)
            return true;
    return false;
}

void DiscoveryProtocol::__discoverInterfaces() {
    struct ifaddrs* ifap;
    int status = getifaddrs(&ifap);
    if (status == 0) {
        struct ifaddrs* cur = ifap;
        while (cur) {
            if (cur->ifa_addr != NULL) {
                int family = cur->ifa_addr->sa_family;
                if (family == AF_INET) {
                    DiscoveryProtocol::NetworkInterface iface = __parseIFA(cur);
                    if (__isInterfaceAccepted(iface.name)) {
                        auto it = m_interface_map.find(iface.name);
                        if (it == m_interface_map.end()) { // new interface
                            if (iface.create() == 0)
                                m_interface_map.insert(std::pair<std::string, DiscoveryProtocol::NetworkInterface>(iface.name, iface));
                        } else { // interface already exists, see if it changed config
                            NetworkInterface old_iface = it->second;
                            if (old_iface.address != iface.address || old_iface.netmask != iface.netmask || old_iface.broadcast != iface.broadcast) {
                                LOG(info) << "Interface " << iface.name << " changed...";
                                m_interface_map.erase(old_iface.name);
                                old_iface.destroy();
                                if (iface.create() == 0)
                                    m_interface_map.insert(std::pair<std::string, DiscoveryProtocol::NetworkInterface>(iface.name, iface));
                            }
                        }
                    }
                }
            }
            cur = cur->ifa_next;
        }
    }
}

void DiscoveryProtocol::__discoveryThread() {
    bool is_running = true;
    std::vector<DiscoveryProtocol::NetworkInterface> interfaces;
    char discovery_buf[MAX_DISCOVERY_RESPONSE_BUFFER];

    while (is_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);

        int maxfd = m_event_pipe_read_end;
        FD_SET(m_event_pipe_read_end, &read_fds);

        for (int i = 0; i < interfaces.size(); i++) {
            FD_SET(interfaces[i].broadcast_socket, &read_fds);
            maxfd = std::max(maxfd, interfaces[i].broadcast_socket);
        }

        int ret = select(maxfd + 1, &read_fds, NULL, NULL, NULL);
        if (ret > 0) {
            if (FD_ISSET(m_event_pipe_read_end, &read_fds)) {
                // clear all (128 is a magic number, put anything...)
                uint8_t tmp_buf[128];
                int nread = __robust_read(m_event_pipe_read_end, tmp_buf, 128);
                if (nread < 0) {
                    LOG(fatal) << "DiscoveryProtocol thread failed to read from the event pipe";
                    break;
                }

                int i = 0;
                for (i = 0; i < nread; i++) {
                    if (tmp_buf[i] == EVENT_KILL)
                        is_running = false;
                }
                if (is_running) {
                    interfaces.clear();
                    m_interfaces_lock.lock();
                    for (auto it = m_interface_map.begin(); it != m_interface_map.end(); it++) {
                        it->second.send_broadcast();
                        interfaces.push_back(it->second);
                    }
                    m_interfaces_lock.unlock();
                }
            } else {
                for (int i = 0; i < interfaces.size(); i++) {
                    if (FD_ISSET(interfaces[i].broadcast_socket, &read_fds)) {
                        struct sockaddr_in sender_addr;
                        socklen_t addr_size = sizeof(sender_addr);
                        int nread = __robust_recvfrom(interfaces[i].broadcast_socket, (uint8_t*)discovery_buf, MAX_DISCOVERY_RESPONSE_BUFFER - 1, 0, (struct sockaddr*)&sender_addr, &addr_size);
                        if (nread >= 4 && nread >= discovery_buf[3] + 4) {
                            discovery_buf[nread] = 0;
                            if (m_callback) {
                                std::string ip = __read_ip(sender_addr);
                                std::string name = std::string(&discovery_buf[4]);
                                std::string data = "";
                                if (name.find(":") != std::string::npos) {
                                    data = name.substr(name.find(':') + 1);
                                    name = name.substr(0, name.find(':'));
                                }
                                m_interfaces_lock.lock();
                                m_callback(interfaces[i].name, name, ip, discovery_buf[2], data);
                                m_interfaces_lock.unlock();
                            }
                        }
                    }
                }
            }
        } else
            LOG(warning) << "DiscoveryProtocol Select failed: " << ret << " (errno=" << errno << ")";
    }

    LOG(info) << "Discovery protocol thread shutting down...";
}

void DiscoveryProtocol::__notify(EVENT_TYPE event) {
    uint8_t buf[1] = {(uint8_t)event};
    __robust_write(m_event_pipe_write_end, buf, 1);
}

int DiscoveryProtocol::Initialize() {
    m_broadcast_interfaces = ConfigManager::get<std::vector<std::string>>("discovery-interfaces");

    int pipe_ends[2];
    if (pipe(pipe_ends) != 0)
        return -1;
    m_event_pipe_read_end = pipe_ends[0];
    m_event_pipe_write_end = pipe_ends[1];

    m_listener_thread = std::thread(__discoveryThread);

    LOG(info) << "Discovery protocol ready";

    return 0;
}

void DiscoveryProtocol::Cleanup() {
    __notify(EVENT_KILL);

    if (m_listener_thread.joinable())
        m_listener_thread.join();

    if (m_event_pipe_read_end != -1)
        close(m_event_pipe_read_end);
    if (m_event_pipe_write_end != -1)
        close(m_event_pipe_write_end);

    m_event_pipe_read_end = m_event_pipe_write_end = -1;

    LOG(info) << "Discovery protocol shut down";
}

void DiscoveryProtocol::InitiateDiscovery(DiscoveryCallback callback) {
    LOG(debug) << "Discovery request initiated";
    m_interfaces_lock.lock();
    m_callback = callback;
    __discoverInterfaces();
    m_interfaces_lock.unlock();
    __notify(EVENT_BROADCAST);
}