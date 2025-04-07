#ifndef NETWORKNODE_H
#define NETWORKNODE_H

extern "C" {
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include "read_write.h"
}

#include <bits/stdc++.h>
#include "config.h"

class Node {
public:
    Node(const char* host, uint16_t port);
    std::string get_ip() const;
    uint16_t get_port() const;
private:
    std::string ip;
    uint16_t port;
};

class NetworkNode : public Node{
public:
    std::vector<Node> connectedNodes;
    uint32_t no_nodes;

    NetworkNode(const char* host, uint16_t port, int my_fd);

    std::pair<bool, sockaddr_in> send_message(const char* host, uint16_t port, uint8_t message);
    void send_HELLO(const char* host, uint16_t port);
    void send_HELLO_REPLY(const char* host, uint16_t port);
    void send_CONNECT(const char* host, uint16_t port);
    void send_ACK_CONNECT(const char* host, uint16_t port);
    void send_DELAY_REQUEST(const char *target_host, uint16_t target_port);
    void send_DELAY_RESPONSE(const char *target_host, uint16_t target_port);
    void send_SYNC_START(const char *target_host, uint16_t target_port);

    void receive_HELLO_REPLY();
    void receive_DELAY_REQUEST();
    void receive_DELAY_RESPONSE();
    void receive_SYNC_START();
    void receive_message();

    bool synchronize();

    bool addNode(const char* host, uint16_t port);

    uint64_t get_current_timestamp();
    void send_time_stamp(const char* host, uint16_t port);

    size_t connected_nodes_size() const;
    int get_fd() const;
    bool wants_to_synchronize();

    void report_nodes();

private:
    std::chrono::time_point<std::chrono::system_clock> startTime;
    uint8_t synchronized_lvl;
    uint8_t is_synchronized;
    int my_fd;
    bool still_synchronizes;
    uint64_t offset;
    uint8_t sync_master_id; //to by trzeba jakos inaczej
    SynchContext synch_state;
};

#endif //NETWORKNODE_H
