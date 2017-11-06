#pragma once

#include <sys/socket.h>
#include <netinet/in.h>

#include <unordered_map>
#include <thread>
#include <mutex>
#include <vector>

/** Port on which the broadcasting protocol runs */
#define BROADCAST_PORT 7991

/** Default port for the middleware */
#define MIDDLEWARE_DEFAULT_PORT 7990

/** Maximum length of a discovery response */
#define MAX_DISCOVERY_RESPONSE_BUFFER 512

/**
 * A callback function that is invoked when a device is discovered. Params:
 * - interface name
 * - device name
 * - device ip
 * - device type
 * - device data
 */
typedef void (*DiscoveryCallback) (std::string, std::string, std::string, int, int, std::string);

/**
 * The DiscoveryProtocol provides a toolset to discover devices using the discovery ptorocol
 */
class DiscoveryProtocol {
    /**
     * Represents a network interface
     */
    struct NetworkInterface {
        /** Interface name */
        std::string name;
        /** Interface flags */
        unsigned int flags;

        /** Interface IPv4 address */
        std::string address;
        /** Interface address sockaddr_in */
        struct sockaddr_in addr_struct;

        /** Interface netmask */
        std::string netmask;
        /** Interface netmask sockaddr_in */
        struct sockaddr_in netmask_struct;

        /** Interface broadcast address */
        std::string broadcast;
        /** Interface broadcast address sockaddr_in */
        struct sockaddr_in broadcast_struct;

        /** UDP socket thet can be used to broadcast on this interface */
        int broadcast_socket;

        /** Initializes socket */
        NetworkInterface();

        /** Creates broadcast socket */
        int create();
        /** Destroys broadcast socket */
        void destroy();
        /** Sends a broadcast on the socket */
        void send_broadcast();
        /** Appends bytes to the read buffer and attempts to parse the commands */
        void onRead(char* buf, size_t buflen, DiscoveryCallback cb);
    };

    /**
     * Represents an event to be sent via __notify() to worker thread
     */
    enum EVENT_TYPE {
        /** Kills the thread */
        EVENT_KILL = 0,
        /** New broadcast initiated (refresh sockets) */
        EVENT_BROADCAST = 1,
    };

    /** Interfaces on which broadcasting is allowed */
    static std::vector<std::string> m_broadcast_interfaces;
    /** interface name -> interface info map (currently found on system) */
    static std::unordered_map<std::string, DiscoveryProtocol::NetworkInterface> m_interface_map;
    /** Currently registered callback for device discovery */
    static DiscoveryCallback m_callback;
    /** A thread handle for the worker thread that broadcasts and listens */
    static std::thread m_listener_thread;
    /** A lock for m_interface_map and m_callback access */
    static std::mutex m_interfaces_lock;
    /** Read end of the event pipe to send to the worker thread */
    static int m_event_pipe_read_end;
    /** Write end of the event pipe to send to the worker thread */
    static int m_event_pipe_write_end;

    /**
     * Parses an ifaddrs* object into a NetworkInterface , initializing:
     * - name
     * - flags
     * - address and addr_struct
     * - netmask and netmask_struct
     * - broadcast and broadcast_struct
     * @param  a Interface to parse
     * @return   Parsed interface
     */
    static DiscoveryProtocol::NetworkInterface __parseIFA(struct ifaddrs* a);

    /**
     * Checks if a network interface is listed in m_broadcast_interfaces, meaning
     * it is allowed to be used for broadcasting
     * @param  name Name of the interface
     * @return      true if the interface is listed, false otherwise
     */
    static bool __isInterfaceAccepted(std::string name);

    /**
     * Fills the m_interface_map from the system and initiates a discovery procedure
     */
    static void __discoverInterfaces();

    /**
     * Worker thread entry point
     */
    static void __discoveryThread();

    /**
     * Notifies the worker thread
     * @param event Event to send to the worker thread
     */
    static void __notify(EVENT_TYPE event);

public:
    /**
     * Initialize the discovery protocol
     * @return 0 on success, negative value otherwise
     */
    static int Initialize();

    /**
     * Kills the worker thread and frees resources
     */
    static void Cleanup();

    /**
     * Initiates a discovery request. When a response is captured, the
     * OnDeviceDiscovered method will be called on the provided callback object.
     * @param callback A DiscoveryCallback object that will receive the discovery
     *                 responses via OnDeviceDiscovered()
     */
    static void InitiateDiscovery(DiscoveryCallback callback);
};
