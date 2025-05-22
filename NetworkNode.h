#ifndef NETWORKNODE_H
#define NETWORKNODE_H

extern "C" {
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
}

#include <bits/stdc++.h>
#include "config.h"

typedef enum  {
    IDLE,                   //is not synchronizing with any node [no SYNCH START was received/accepted]
    WAITING_FOR_RESPONSE   //is synchronizing, sent DELAY_REQUEST and waits for DELAY_RESPONSE
  } SynchRecvState;

typedef enum {
    NONE,                 //didn't send any SYNCH_START and is not expecting DELAY_REQUEST
    WAITING_FOR_REQUEST   //sent SYNCH_START and now waits if any node wants to synchronize
  } SynchSdState;

// Synchronization context for tracking current sync process
typedef struct {
    const char* sync_partner_id;          // ID of the node we're synchronizing with
    uint16_t sync_partner_port;           // Port of the node we're synchronizing with
    uint8_t partner_sync_level;           // Synchronization level of the partner
    uint32_t T1;                          // Timestamp from SYNC_START
    uint32_t T2;                          // Local timestamp when SYNC_START received
    uint32_t T3;                          // Local timestamp when DELAY_REQUEST sent
    uint32_t T4;                          // Timestamp from DELAY_RESPONSE
    SynchRecvState synch_receiver_state;  // Current state in the sync receive process
    SynchSdState synch_sender_state;      // Current state in the sync start process
    uint64_t last_sync_time;              // Time of last successful sync
    uint64_t waits_from;                  //last time any message has been received [DELAY_REQUEST/DELAY RESPONSE/...]
} SynchContext;


class Node {

public:
    Node(const char* host, uint16_t port);
    [[nodiscard]] std::string get_ip() const;
    [[nodiscard]] uint16_t get_port() const;

    bool operator<(const Node& other) const {
        return std::tie(port, ip) < std::tie(other.port, other.ip);
    }

    bool operator==(const Node& other) const {
        return std::tie(port, ip) == std::tie(other.port, other.ip);
    }

private:
    std::string ip;
    uint16_t port;
};

namespace std {
template<>
struct hash<Node> {
    std::size_t operator()(const Node& n) const {
        return std::hash<std::string>()(n.get_ip()) ^ (std::hash<uint16_t>()(n.get_port()) << 1);
    }
};
}

class NetworkNode : public Node{
public:
    std::set<Node> connectedNodes;
    std::set<Node> pendingHELLOS; //to know if HELLO_REPLY is expected or no
    std::set<Node> pendingCONNECTS; //to know if CONNECT_ACK is expected or no
    std::unordered_map<Node, uint64_t> last_SYNCH_sent; //to check if response to SYNCH START is expected
    std::unordered_map<Node, uint64_t> last_SYNCH_received; //to check for timeout with partner

    NetworkNode(const char* host, uint16_t port, int my_fd);

    bool send_message(const char* host, uint16_t port, uint8_t message);
    void send_HELLO(const char* host, uint16_t port);
    void send_HELLO_REPLY(const char* host, uint16_t port, uint8_t* buffer, size_t buffer_size);
    bool send_CONNECT(const char* host, uint16_t port);
    void send_ACK_CONNECT(const char* host, uint16_t port);
    void send_DELAY_REQUEST(const char *target_host, uint16_t target_port);
    void send_DELAY_RESPONSE(const char *target_host, uint16_t target_port);
    void send_SYNC_START();
    void send_TIME(const char* target_host, uint16_t target_port);

    void receive_HELLO_REPLY(const char* senders_address, uint16_t senders_port, uint8_t* buffer, size_t buffer_size);
    void receive_SYNC_START(const char* senders_address, uint16_t senders_port, uint8_t* buffer, size_t buffer_size);
    void receive_DELAY_REQUEST(const char*senders_address, uint16_t senders_port, uint8_t* buffer, size_t buffer_size);
    void receive_DELAY_RESPONSE(const char* senders_address, uint16_t senders_port, uint8_t* buffer, size_t buffer_size);
    void receive_LEADER(uint8_t* buffer, size_t buffer_size);
    void receive_message();

    bool addNode(const char* host, uint16_t port);

    uint64_t get_current_timestamp();
    uint64_t get_timestamp();
    uint64_t get_latest_SYNCH_START_sending();

    void timeout_check();

    bool node_known(const char* node_ip, uint16_t node_port) const;

    void report_nodes() const;

private:
    int my_fd;
    std::chrono::time_point<std::chrono::system_clock> startTime;
    uint8_t synchronized_lvl;
    uint8_t is_synchronized;
    int64_t offset;
    Node sync_master_id;
    SynchContext synch_state;
    uint64_t latest_SYNCH_START_sending;
};

#endif //NETWORKNODE_H
