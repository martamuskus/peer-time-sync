#include "NetworkNode.h"
#include <iostream>

//todo line 290!!!
//todo 2: wybieranie LIDERA

extern "C" {
#include "read_write.h"
#include <endian.h>
}

Node::Node(const char* host, uint16_t p) {
    ip = host;
    port = p;
}

std::string Node::get_ip() const{
    return ip;
}

uint16_t Node::get_port() const{
    return port;
}

bool NetworkNode::wants_to_synchronize() {
    return still_synchronizes;
}


NetworkNode::NetworkNode(const char* host, uint16_t p, int fd): Node(host, p), is_synchronized(0), sync_master_id(0) {
    startTime = std::chrono::system_clock::now();
    synchronized_lvl = UINT8_MAX;
    my_fd = fd;
    no_nodes = 0;
    still_synchronizes = true;
    offset = 0;
    synch_state.state = SYNC_NONE;
}

int NetworkNode::get_fd() const {
    return my_fd;
}

size_t NetworkNode::connected_nodes_size() const{
    return connectedNodes.size();
}

void NetworkNode::report_nodes() {
    for (auto& node: connectedNodes) {
        fprintf(stderr, "host:%s port:%d\n", node.get_ip().c_str(), node.get_port());
    }
}

//returns timestamp in millis
uint64_t NetworkNode::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - startTime).count();
}

std::pair<bool, sockaddr_in> NetworkNode::send_message(const char *target_host, uint16_t target_port, uint8_t message) {
    struct sockaddr_in server_addr;
    //memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_host, &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "error converting address");
        return std::make_pair(false, server_addr);
    }

    //instead of connect
    ssize_t send_bytes = sendto(my_fd, &message, sizeof(message), 0,
                                (struct sockaddr*)&server_addr, sizeof(server_addr));


    //ssize_t send_bytes = writen(my_fd, &message, sizeof message);
    if ((size_t)send_bytes < sizeof message) {
         fprintf(stderr, "error sending message");
         return std::make_pair(false, server_addr);
    }

    return std::make_pair(true, server_addr);
}


void NetworkNode::send_HELLO(const char *target_host, uint16_t target_port) {
    bool ok = send_message(target_host, target_port, HELLO).first;
    if (!ok) {
        fprintf(stderr, "error sending HELLO to %s %d \n", target_host, target_port);
    }
    else {
        fprintf(stderr, "sent HELLO to %s %d \n", target_host, target_port);
    }
}

void NetworkNode::send_HELLO_REPLY(const char *target_host, uint16_t target_port) {
    struct sockaddr_in server_addr;
    auto p = send_message(target_host, target_port, HELLO_REPLY);
    if (!p.first) return;
    server_addr = p.second;
    //number of records
    uint16_t count = htons(no_nodes); //in net order
    ssize_t send_bytes = sendto(my_fd, &count, sizeof(count), 0,
                               (struct sockaddr*)&server_addr, sizeof(server_addr));
    if ((size_t)send_bytes < sizeof(count)) {
        fprintf(stderr, "error sending count");
        return;
    }


    size_t offset = 0;
    size_t buffer_size = 0;

    buffer_size += connected_nodes_size() * (sizeof(uint8_t) + sizeof(in_addr) + sizeof(uint16_t));

    // for (const auto& node : connectedNodes) {
    //     buffer_size += 1;          // 1 byte for address length
    //     buffer_size += sizeof(in_addr);  // 4 bytes for IPv4 address
    //     buffer_size += sizeof(uint16_t); // 2 bytes for port
    // }

    std::vector<uint8_t> buffer(buffer_size);

    for (const auto& node : connectedNodes) {
        struct in_addr ip_addr;
        inet_pton(AF_INET, node.get_ip().c_str(), &ip_addr);

        uint8_t addr_length = 4;
        buffer[offset++] = addr_length;

        memcpy(&buffer[offset], &ip_addr, addr_length);
        offset += addr_length;

        uint16_t port = htons(node.get_port());
        memcpy(&buffer[offset], &port, sizeof(port));
        offset += sizeof(port);
    }

    //sending to the buffer - PROBABLY CHUNKS WOULD BE NICE
    ssize_t total_sent = sendto(my_fd, buffer.data(), buffer_size, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if ((size_t)total_sent < buffer_size) {
        fprintf(stderr, "error sending buffer");
        return;
    }
    fprintf(stderr, "sent %lu bytes of HELLO_REPLY to %s %d\n", total_sent, target_host, target_port);
}

void NetworkNode::send_CONNECT(const char *target_host, uint16_t target_port) {
    send_message(target_host, target_port, CONNECT);
}

void NetworkNode::send_ACK_CONNECT(const char *target_host, uint16_t target_port) {
    send_message(target_host, target_port, ACK_CONNECT);
}

void NetworkNode::send_DELAY_REQUEST(const char *target_host, uint16_t target_port) {
    auto ok = send_message(target_host, target_port, DELAY_REQUEST);
    if (ok.first) {
        fprintf(stderr, "sent DELAY REQUEST TO %s, %d", target_host, target_port);
    }
}

void NetworkNode::send_DELAY_RESPONSE(const char *target_host, uint16_t target_port) {
    struct sockaddr_in server_addr;
    auto result = send_message(target_host, target_port, DELAY_RESPONSE);
    if (!result.first) return;
    server_addr = result.second;

    //sending synchronized lvl
    uint8_t synch = synchronized_lvl;
    ssize_t send_bytes = sendto(my_fd, &synch, sizeof(synch), 0,
                                (struct sockaddr*)&server_addr, sizeof(server_addr));

    if ((size_t)send_bytes < sizeof synch) {
        fprintf(stderr, "error sending synchronized level");
        return;
    }

    //sending current timestamp
    uint64_t timestamp = get_current_timestamp();
    uint64_t timestamp_net = htobe64(timestamp);
    send_bytes = sendto(my_fd, &timestamp_net, sizeof(timestamp_net), 0,
                                (struct sockaddr*)&server_addr, sizeof(server_addr));

    if ((size_t)send_bytes < sizeof timestamp_net) {
        fprintf(stderr, "error sending message");
        return;
    }

    fprintf(stderr, "sent DELAY REQUEST TO %s, %d", target_host, target_port);
}


bool NetworkNode::synchronize() {
    fprintf(stderr, "unimplemented");
    return true;
}

void NetworkNode::send_time_stamp(const char* target_host, uint16_t target_port) {
    struct sockaddr_in server_addr;
    auto result = send_message(target_host, target_port, TIME);
    if (!result.first) return;
    server_addr = result.second;

    uint8_t synch = synchronized_lvl;
    ssize_t send_bytes = sendto(my_fd, &synch, sizeof(synch), 0,
                                (struct sockaddr*)&server_addr, sizeof(server_addr));

    if ((size_t)send_bytes < sizeof synch) {
        fprintf(stderr, "error sending synchronized lvl");
        return;
    }

    uint64_t timestamp = get_current_timestamp();
    if (synchronized_lvl < UINT8_MAX) {
        //the node is synchronized
        timestamp += offset;
    }

    uint64_t timestamp_net = htobe64(timestamp);

    send_bytes = sendto(my_fd, &timestamp_net, sizeof(timestamp_net), 0,
                                (struct sockaddr*)&server_addr, sizeof(server_addr));

    if ((size_t)send_bytes < sizeof timestamp_net) {
        fprintf(stderr, "error sending timestamp");
        return;
    }

    fprintf(stderr, "SEND TIME to %s, %d", target_host, target_port);
}

bool NetworkNode::addNode(const char* host, uint16_t port) {
    Node node = Node(host, port);
    if (host == get_ip()) return false;
    for (const auto& n: connectedNodes) { //node is identified by its ip
        if (n.get_ip().c_str() == host) return false;
    }

    connectedNodes.push_back(node);
    no_nodes++;
    return true;
}

void NetworkNode::receive_DELAY_RESPONSE() {

}

//gets nodes and then sends connect
//this function is called right after the node reads that it has received HELLO_REPLY message
void NetworkNode::receive_HELLO_REPLY() {
    std::vector<Node> received_nodes;
    //reading node count
    uint16_t received_count_net;
    ssize_t read_bytes = recv(my_fd, &received_count_net, sizeof(received_count_net), 0);
    uint16_t received_count = ntohs(received_count_net);
    if ((size_t)read_bytes < sizeof received_count_net) {
        fprintf(stderr, "Failed to read node count");
        return;
    }
    size_t max_buffer_size = received_count * (1 + INET_ADDRSTRLEN + sizeof(uint16_t));

    std::vector<uint8_t> buffer(max_buffer_size);
    size_t bytes_read = 0;
    size_t remaining = max_buffer_size;
    uint32_t current_pos = 0;

    while (remaining > 0) {
        read_bytes = recv(my_fd, &buffer[current_pos], remaining, 0);
        bytes_read += read_bytes;
        current_pos += read_bytes;
        remaining -= read_bytes;

        uint chunk_size = sizeof(uint8_t) + sizeof(uint16_t) + sizeof(in_addr);
        if (bytes_read >= (received_count * chunk_size)) {
            break;
        }
    }

    current_pos = 0;
    for (uint32_t i = 0; i < received_count; i++) {
        uint8_t addr_length = buffer[current_pos];
        current_pos += sizeof(uint8_t);

        struct in_addr ip_addr;
        memcpy(&ip_addr, &buffer[current_pos], addr_length);
        current_pos += addr_length;

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);

        uint16_t port_net;
        memcpy(&port_net, &buffer[current_pos], sizeof(port_net));
        current_pos += sizeof(port_net);
        uint16_t port = ntohs(port_net);

        received_nodes.push_back(Node(ip_str, port)); //idk if connect should be send to all of these nodes
        //fprintf(stderr, "Adding node with IP: %s and port: %d\n", ip_str, port);
    }

    //sending CONNECT requests
    for (auto& node : received_nodes) {
        send_CONNECT(node.get_ip().c_str(), node.get_port());
    }
}


void NetworkNode::receive_SYNC_START(const char* host, uint16_t port) {
    if (node->sync_ctx.state != SYNC_IDLE && sender_id != node->sync_ctx.sync_partner_id) {
        return;
    }

    // Check if sender is known (implementation dependent)
    if (!is_node_known(sender_id)) {
        return;
    }

    // Check if sender's sync level is < 254
    if (sender_sync_level >= 254) {
        return;
    }

    // Check our relationship with the sender
    bool is_sync_with_sender = (node->sync_master_id == sender_id && node->is_synchronized);

    // Apply synchronization rules
    bool should_sync = false;
    if (is_sync_with_sender) {
        // For nodes we're synchronized with
        should_sync = (sender_sync_level < node->sync_level);
    } else {
        // For nodes we're not synchronized with
        should_sync = (sender_sync_level + 2 <= node->sync_level);
    }

    if (!should_sync) {
        return;
    }

    // Record timestamps and prepare for next step
    uint32_t t2 = get_local_time();

    // Save synchronization context
    node->sync_ctx.sync_partner_id = sender_id;
    node->sync_ctx.partner_sync_level = sender_sync_level;
    node->sync_ctx.t1 = sender_timestamp;
    node->sync_ctx.t2 = t2;
    node->sync_ctx.state = SYNC_WAITING_FOR_REQUEST;

    // Send DELAY_REQUEST
    uint32_t t3 = get_local_time();
    node->sync_ctx.t3 = t3;
    send_message(sender_id, DELAY_REQUEST, 0, 0); // No timestamp needed for request
    fprintf(stderr, "unimplemented!!");
}


//this function is called just to receive a first message to know what type
//of communicate it is. thanks to that we know the ip address and port
//of the one sending
void NetworkNode::receive_message() {
    struct sockaddr_in sender_addr;
    socklen_t addr_len = (socklen_t) sizeof(sender_addr);
    uint8_t message;

    ssize_t bytes_received = recvfrom(my_fd, &message, sizeof(message), 0,
                                     (sockaddr*)&sender_addr, &addr_len);

    if (bytes_received < 0) {
        fprintf(stderr, "no message received");
        return;
    }

    char const *senders = inet_ntoa(sender_addr.sin_addr);
    uint16_t senders_port = ntohs(sender_addr.sin_port);

    //checking what type of message was received
    switch (message) {
        case HELLO: //replying to HELLO
            fprintf(stderr, "received HELLO from %s %d \n", senders, senders_port);
        send_HELLO_REPLY(senders, senders_port);
        break;
        case HELLO_REPLY: //getting data about connected nodes
            fprintf(stderr, "received HELLO_REPLY from %s %d \n", senders, senders_port);
        receive_HELLO_REPLY();
        break;
        case CONNECT: {
            //adding the new node to the list of connected nodes and reassuring the sender that the message has been received
            fprintf(stderr, "received CONNECT from %s %d \n", senders, senders_port);
            if (addNode(senders, senders_port)) {
                send_ACK_CONNECT(senders, senders_port);
            }
            break;
        }
        case ACK_CONNECT:
            //adding the node which sent ACK_CONNECT to the list
            fprintf(stderr, "received ACK_CONNECT from %s %d \n", senders, senders_port);
            addNode(senders, senders_port);
            break;
        case GET_TIME:
            send_time_stamp(senders, senders_port); //this will be TIME communicate in the future
            break;
        case DELAY_REQUEST:
            send_DELAY_RESPONSE(senders, senders_port);
            break;
        case DELAY_RESPONSE:
            //continues to synchronize, calculating offset and updating synchronized lvl
            break;
        case SYNC_START:
            receive_SYNC_START();
            break;
        default:
            fprintf(stderr, "incorrect message");
            break;
    }
}



