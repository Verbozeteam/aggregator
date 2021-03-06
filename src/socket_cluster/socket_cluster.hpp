#pragma once

#include "aggregator_clients/discovery_protocol.hpp"

// openSSL
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <json.hpp>
using json = nlohmann::json;

#include <unordered_map>
#include <thread>
#include <mutex>
#include <shared_mutex>

typedef std::shared_ptr<class SocketClient> SocketClientPtr;

/**
 * The SocketCluster facilitates the management of SocketClient's by running and
 * maintaining a thread that performs a select loop on the SocketClient's FDs.
 *
 * The cluster is implemented as follows:
 * While thread is alive:
 *     - create read FDs (all registered clients + eventfd) and write FDs (all clients that have > 0 write buffers)
 *     - perform a select on the FDs
 *     - after select returns:
 *         - serve any reads or writes to clients
 *         - read the eventfd and discard the value
 *
 * When a client fills some data in its write buffer, it must Notify() the SocketCluster
 * so that it breaks out of the select
 */
class SocketCluster {
    friend class SocketClient;

    /** SSL context */
    static SSL_CTX* m_ssl_context;

    /** thread runs while true */
    static bool m_is_alive;
    /** used to notify the select */
    static int m_event_pipe_read_end;
    /** used to notify the select */
    static int m_event_pipe_write_end;
    /** shared mutex to protect m_clients and m_clients_by_ip */
    static std::shared_timed_mutex m_clients_mutex;
    /** fd -> SocketClientPtr map */
    static std::unordered_map<int, SocketClientPtr> m_clients;
    /** identifier -> SocketClientPtr map (same SocketClient* as above) */
    static std::unordered_map<std::string, SocketClientPtr> m_clients_by_id;
    /** thread running the select loop */
    static std::thread m_server_thread;

    /**
     * Entry point of the thread that performs the select on the clients
     */
    static void __thread_entry();

public:
    /**
     * Initializes the cluster
     * @return 0 on success, negative value on failure
     */
    static int Initialize();

    /**
     * Performs a join() on the select thread
     */
    static void WaitForCompletion();

    /**
     * Cleans up all resources
     */
    static void Cleanup();

    /**
     * (THREAD SAFE) Notifies the cluster to break out of the select loop to update the FDs
     */
    static void Notify();

    /**
     * (THREAD SAFE) Kills the select thread
     */
    static void Kill();

    /**
     * (THREAD SAFE) Registers a SocketClientPtr
     * @param client SocketClientPtr to register
     */
    static void RegisterClient(SocketClientPtr client);

    /**
     * (THREAD SAFE) Deregisters a SocketClientPtr
     * @param client SocketClientPtr to deregister
     */
    static void DeregisterClient(SocketClientPtr client);

    /**
     * Returns whether or not a client with the given ID is registered
     * @param  id ID to check against
     * @return    true iff a client is registered with the given ID
     */
    static bool IsClientRegistered(DISCOVERED_DEVICE device);

    /**
     * Retrieves a registered client by its ID
     * @param  id ID of the client
     * @return    registered client (nullptr if no client with the given ID is registered)
     */
    static SocketClientPtr GetClient(std::string id);

    /**
     * @return  a list of clients registered
     */
    static std::vector<SocketClientPtr> GetClientsList();

    /**
     * Checks if a device requires an SSL connection
     * @param  device  Device to check
     */
    static bool DeviceRequiresSecureConnection(DISCOVERED_DEVICE device);
};


/**
 * Abstract base class for a client to be managed by SocketCluster
 */
class SocketClient {
    friend class SocketCluster;

    /** fd for the socket */
    int m_client_fd;
    /** SSL object for m_client_fd */
    SSL* m_ssl;
    /** mutex to protect modifying the write buffer */
    std::mutex m_write_buffer_mutex;
    /** pending write buffer */
    std::vector<uint8_t> m_write_buffer;
    /** pending read buffer */
    std::vector<uint8_t> m_read_buffer;

    /**
     * Connects a socket to the given address
     * @param  ip   IP to connect to
     * @param  port port to connect to
     * @return      socket FD, or negative value on failure
     */
    static int __openConnection(std::string ip, int port);

    /**
     * Called when there is pending data to be read from m_client_fd
     * @return whether or not the client should remain connected/registered
     */
    bool OnReadingAvailable();

    /**
     * Called when there is room for data to be written to m_client_fd (and hopefully won't block)
     * @return whether or not the client should remain connected/registered
     */
    bool OnWritingAvailable();

protected:
    /**
     * Initializes variables
     */
    SocketClient(int fd, DISCOVERED_DEVICE device);

    std::string m_ip; // IP address of the client (x.x.x.x format)
    int m_port; // Port of the client
    std::string m_identifier; // Name of the device

public:
    /**
     * Creates a socket client and registers it in the system
     * @param ip    Middleware server IP
     * @param port  Middleware port
     * @returns     The new client created, nullptr on failure
     */
    template<typename T>
    static SocketClientPtr Create(DISCOVERED_DEVICE device) {
        if (SocketCluster::DeviceRequiresSecureConnection(device) && !SocketCluster::m_ssl_context)
            return nullptr;

        int fd = __openConnection(device.ip, device.port);
        if (fd <= 0)
            return nullptr;
        SocketClientPtr p = SocketClientPtr(new T(fd, device));
        SocketCluster::RegisterClient(p);
        return p;
    }

    /**
     * Frees resources (m_client_fd)
     */
    virtual ~SocketClient();

    /**
     * (SAFE) Writes a JSON-formatted message to the client socket
     * @param msg JSON-formatted message to write
     */
    virtual void Write(json msg);

    /**
     * Can be implemented by a derived class to perform an action when a full JSON
     * message has been read from the socket
     * @param  msg JSON message found
     * @return     whether or not the client should remain connected/registered
     */
    virtual bool OnMessage(json msg);
};
