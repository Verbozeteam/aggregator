#pragma once

#include <json.hpp>
using json = nlohmann::json;

#include <unordered_map>
#include <thread>
#include <mutex>
#include <shared_mutex>

typedef std::shared_ptr<class SocketClient> SocketClientPtr;

/** Just a robust write() call */
int __robust_write(int fd, uint8_t* buf, size_t buflen);

/** Just a robust send() call */
int __robust_send(int fd, uint8_t* buf, size_t buflen, int flags=0);

/** Just a robust read() call */
int __robust_read(int fd, uint8_t* buf, size_t maxread);

/** Just a robust recv() call */
int __robust_recv(int fd, uint8_t* buf, size_t maxread, int flags=0);

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

    static bool m_is_alive; // thread runs while true
    static int m_event_pipe_read_end; // used to notify the select
    static int m_event_pipe_write_end; // used to notify the select
    static std::shared_timed_mutex m_clients_mutex; // shared mutex to protect m_clients and m_clients_by_ip
    static std::unordered_map<int, SocketClientPtr> m_clients; // fd -> SocketClientPtr map
    static std::unordered_map<std::string, SocketClientPtr> m_clients_by_ip; // ip -> SocketClientPtr map (same SocketClient* as above)
    static std::thread m_server_thread; // thread running the select loop

    /**
     * Entry point of the thread that performs the select on the clients
     * @param max_connections [description]
     */
    static void __thread_entry(int max_connections);

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
};


/**
 * Abstract base class for a client to be managed by SocketCluster
 */
class SocketClient {
    friend class SocketCluster;

    int m_client_fd; // fd for the socket
    std::mutex m_write_buffer_mutex; // mutex to protect modifying the write buffer
    std::vector<uint8_t> m_write_buffer; // pending write buffer
    std::vector<uint8_t> m_read_buffer; // pending read buffer

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
    SocketClient(int fd, std::string ip);

    std::string m_client_ip; // IP address of the client (x.x.x.x format)

public:
    template<typename T>
    static SocketClientPtr Create(std::string ip, int port) {
        int fd = __openConnection(ip, port);
        if (fd <= 0)
            return nullptr;
        SocketClientPtr p = SocketClientPtr(new T(fd, ip));
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
