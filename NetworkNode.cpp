#include "NetworkNode.h"
#include "err.h"

extern "C" {
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

bool NetworkNode::node_known(const char *node_address, uint16_t node_port) const {
    return count(connectedNodes.begin(), connectedNodes.end(), Node(node_address, node_port)) > 0;
}

NetworkNode::NetworkNode(const char* host, uint16_t p, int fd): Node(host, p), is_synchronized(false), sync_master_id(get_ip().c_str(), get_port()) {
    startTime = std::chrono::system_clock::now();
    synchronized_lvl = UINT8_MAX;
    my_fd = fd;
    offset = 0;
    latest_SYNCH_START_sending = get_current_timestamp();

    synch_state.synch_sender_state = NONE; //hasn't sent any SYNCH_START yet
    synch_state.synch_receiver_state = IDLE; //hasn't begun to synchronize with any node
    synch_state.waits_from = get_current_timestamp();
    synch_state.sync_partner_id = nullptr;
    synch_state.sync_partner_port = -1;
}

uint64_t NetworkNode::get_latest_SYNCH_START_sending() {
    return latest_SYNCH_START_sending;
}

void NetworkNode::report_nodes() const {
    for (auto& node: connectedNodes) {
        fprintf(stderr, "host:%s port:%d\n", node.get_ip().c_str(), node.get_port());
    }
}

/*Returns timestamp according to local not-synchronized clock*/
uint64_t NetworkNode::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - startTime).count();
}

/*Returns timestamp adjusted with offset val*/
uint64_t NetworkNode::get_timestamp() {
    uint64_t timestamp = get_current_timestamp();
    if (synchronized_lvl < UINT8_MAX) {
        timestamp -= offset;
    }
    return timestamp;
}

void NetworkNode::send_TIME(const char* target_host, uint16_t target_port) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_host, &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "ERROR error converting address");
        return;
    }

    uint8_t message = TIME;
    uint8_t synch = synchronized_lvl;
    uint64_t timestamp = get_timestamp();
    uint64_t timestamp_net = htobe64(timestamp);

    size_t offset = 0;
    ssize_t buffer_size = 0;
    buffer_size += (sizeof(message) + sizeof(synch) + sizeof(timestamp_net));

    std::vector<uint8_t> buffer(buffer_size);

    buffer[offset++] = message;

    memcpy(&buffer[offset], &synch, sizeof(synch));
    offset += sizeof(synch);

    memcpy(&buffer[offset], &timestamp_net, sizeof(timestamp_net));

    ssize_t send_bytes = sendto(my_fd, buffer.data(), buffer_size, 0,
                                (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (send_bytes < buffer_size) {
        fprintf(stderr, "ERROR error sending buffer");
    }
}

bool NetworkNode::send_message(const char *target_host, uint16_t target_port, uint8_t message) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_host, &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "ERROR error converting address");
        return false;
    }

    ssize_t send_bytes = sendto(my_fd, &message, sizeof(message), 0,
                                (struct sockaddr*)&server_addr, sizeof(server_addr));


    if ((size_t)send_bytes < sizeof message) {
        fprintf(stderr, "ERROR error sending message");
        return false;
    }
    return true;
}

void NetworkNode::send_HELLO(const char *target_host, uint16_t target_port) {
    bool ok = send_message(target_host, target_port, HELLO);
    if (!ok) {
        fprintf(stderr, "ERROR error sending HELLO to %s %d \n", target_host, target_port);
    }
    else {
        /* to distinguish whether HELLO_REPLY is expected or not */
        pendingHELLOS.insert(Node(target_host, target_port));
    }
}

void NetworkNode::send_HELLO_REPLY(const char* target_host, uint16_t target_port, uint8_t* received, size_t received_bytes) {
    /* checking if message size won't exceed max size of one datagram */
    if (connectedNodes.size() > MAX_NODE_COUNT_TO_SEND) {
        message_err(received, received_bytes);
        return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_host, &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "ERROR error converting address");
        return;
    }

    uint8_t message = HELLO_REPLY;
    uint16_t count = htons(connectedNodes.size());

    size_t offset = 0;
    ssize_t buffer_size = 0;

    buffer_size += sizeof(message);
    buffer_size += sizeof(count);
    buffer_size += connectedNodes.size() * (sizeof(uint8_t) + sizeof(in_addr) + sizeof(uint16_t));

    std::vector<uint8_t> buffer(buffer_size);
    buffer[offset++] = message;

    memcpy(&buffer[offset], &count, sizeof(count));
    offset += sizeof(count);

    for (const auto& node : connectedNodes) {
        if (node.get_ip() == target_host && node.get_port() == target_port) {
            continue; /*sending everything apart from target host*/
        }

        struct in_addr ip_addr;
        if (inet_pton(AF_INET, node.get_ip().c_str(), &ip_addr) <= 0) {
            fprintf(stderr, "ERROR Error creating address.");
            continue;
        }

        uint8_t addr_length = 4;
        buffer[offset++] = addr_length;

        memcpy(&buffer[offset], &ip_addr, addr_length);
        offset += addr_length;

        uint16_t port = htons(node.get_port());
        memcpy(&buffer[offset], &port, sizeof(port));
        offset += sizeof(port);
    }
    ssize_t send_bytes = sendto(my_fd, buffer.data(), buffer_size, 0,
    (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (send_bytes < buffer_size) {
        fprintf(stderr, "ERROR error sending buffer");
    }
    addNode(target_host, target_port); /* nodes are now connected */
}

bool NetworkNode::send_CONNECT(const char *target_host, uint16_t target_port) {
    return send_message(target_host, target_port, CONNECT);
}

void NetworkNode::send_ACK_CONNECT(const char *target_host, uint16_t target_port) {
    send_message(target_host, target_port, ACK_CONNECT);
}

void NetworkNode::send_DELAY_REQUEST(const char *target_host, uint16_t target_port) {
    /* saving time as a reference point for waiting for DELAY RESPONSE */
    synch_state.waits_from = get_current_timestamp();
    if (!send_message(target_host, target_port, DELAY_REQUEST)) {
        fprintf(stderr, "ERROR error sending DELAY_REQUEST TO %s, %d", target_host, target_port);
    }
}

/* sending data after receiving DELAY REQUEST in a correct context */
void NetworkNode::send_DELAY_RESPONSE(const char *target_host, uint16_t target_port) {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(target_port);

    if (inet_pton(AF_INET, target_host, &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "ERROR error converting address");
        return;
    }

    uint8_t message = DELAY_RESPONSE;
    //sending synchronized lvl
    uint8_t synch = synchronized_lvl;
    //sending current timestamp [updated with offset if necessary]
    uint64_t timestamp = get_timestamp();
    uint64_t timestamp_net = htobe64(timestamp);

    size_t offset = 0;
    ssize_t buffer_size = 0;

    buffer_size += (sizeof(message) + sizeof(synch) + sizeof(timestamp_net));
    std::vector<uint8_t> buffer(buffer_size);

    buffer[offset++] = message;

    memcpy(&buffer[offset], &synch, sizeof(synch));
    offset += sizeof(synch);

    memcpy(&buffer[offset], &timestamp_net, sizeof(timestamp_net));
    ssize_t send_bytes = sendto(my_fd, buffer.data(), buffer_size, 0,
                                (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (send_bytes < buffer_size) {
        fprintf(stderr, "ERROR error sending buffer");
    }
}

void NetworkNode::timeout_check() {
    uint64_t current_time = get_current_timestamp();

    if (is_synchronized) {
        uint64_t time_since_sync = current_time - last_SYNCH_received[sync_master_id];
        /*checking if given timeout has already passed [25 seconds with no synch message from master] */
        if (time_since_sync > 25000) {
            synchronized_lvl = 255;
            is_synchronized = false;
            synch_state.synch_receiver_state = IDLE; //restarting the process of synchronization
        }
    }

    /*checking if we've received any info in the last 5 seconds of a pending synchronization process */
    if (current_time - synch_state.waits_from > 2*SYNCH_START_SPAN) {
        synch_state.synch_receiver_state = IDLE;
    }
}

bool NetworkNode::addNode(const char* host, uint16_t port) {
    /* total number of known nodes cannot exceed uint16_t max */
    if (connectedNodes.size() >= UINT16_MAX) {
        return false;
    }

    Node node = Node(host, port);
    if (host == get_ip() && port == get_port()) return false; //not adding out own info
    connectedNodes.insert(node);
    return true;
}


/* Receiving T4 and calculating offset if everything in the communicate is correct */
void NetworkNode::receive_DELAY_RESPONSE(const char* senders_address, uint16_t senders_port, uint8_t* buffer, size_t buffer_size) {
    if (!node_known(senders_address, senders_port)) {
        message_err(buffer, buffer_size);
        return;
    }

    uint64_t current_time = get_current_timestamp();
    uint64_t time_since_any_message = current_time - synch_state.waits_from;

    if (time_since_any_message > 2*SYNCH_START_SPAN) {
        message_err(buffer, buffer_size);
        /*restarting synchronization process and ignoring the message*/
        synch_state.synch_receiver_state = IDLE;
        synch_state.sync_partner_id = nullptr;
        synch_state.sync_partner_port = -1;
        return;
    }

    size_t current_pos = 1;

    if (buffer_size < current_pos + sizeof(uint8_t)) {
        message_err(buffer, buffer_size); /*couldn't read synchronization lvl */
        return;
    }

    uint8_t synchronized = buffer[current_pos];
    current_pos += sizeof(uint8_t);

    uint64_t T4_net;
    if (buffer_size < current_pos + sizeof(uint64_t)) {
        message_err(buffer, buffer_size); /*couldn't read_timestamp */
        return;
    }
    memcpy(&T4_net, &buffer[current_pos], sizeof(T4_net));

    uint64_t T4 = be64toh(T4_net);
    synch_state.T4 = T4;

    /*Incorrect message format.*/
    if (T4 < synch_state.T1) {
        message_err(buffer, buffer_size);
    }

    /*We were waiting for a response and the synchronization level stayed the same and if we received
    this message from the one we were synchronizing with */
    if (synch_state.synch_receiver_state == WAITING_FOR_RESPONSE && synch_state.partner_sync_level == synchronized
        && synch_state.sync_partner_id == senders_address && synch_state.sync_partner_port == senders_port)  {
        synchronized_lvl = synchronized + 1;
        offset = (synch_state.T2 - synch_state.T1 + synch_state.T3 - synch_state.T4)/2;
        is_synchronized = true;
        sync_master_id = Node(senders_address, senders_port);
        synch_state.last_sync_time = get_current_timestamp();
        synch_state.waits_from = get_current_timestamp();
        synch_state.synch_receiver_state = IDLE; //node is ready to synchronize again
    }
    else { /*received message in unexpected context*/
        message_err(buffer, buffer_size);
    }
}

void NetworkNode::send_SYNC_START() {
    uint64_t current_time = get_current_timestamp();
    uint64_t time_since_sending = current_time - latest_SYNCH_START_sending;

    if (time_since_sending < SYNCH_START_SPAN) return;
    if (synchronized_lvl >= 254) return;

    uint32_t any_success = 0;
    for (const auto& node: connectedNodes) {
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(node.get_port());

        if (inet_pton(AF_INET, node.get_ip().c_str(), &(server_addr.sin_addr)) <= 0) {
            fprintf(stderr, "ERROR Error converting address");
            continue;
        }

        size_t offset = 0;
        ssize_t buffer_size = 0;

        uint8_t message = SYNC_START;
        uint8_t synchronized = synchronized_lvl;
        uint64_t timestamp = get_timestamp(); //time updated with offset if node is synchronized
        uint64_t timestamp_net = htobe64(timestamp);

        buffer_size += (sizeof(message) + sizeof(synchronized) + sizeof(timestamp_net));
        std::vector<uint8_t> buffer(buffer_size);
        buffer[offset++] = message;

        memcpy(&buffer[offset], &synchronized, sizeof(synchronized));
        offset += sizeof(synchronized);

        memcpy(&buffer[offset], &timestamp_net, sizeof(timestamp_net));

        ssize_t send_bytes = sendto(my_fd, buffer.data(), buffer_size, 0,
                              (struct sockaddr*)&server_addr, sizeof(server_addr));

        if (send_bytes < buffer_size) {
            fprintf(stderr, "ERROR Error sending buffer");
            continue;
        }
        any_success++;

        if (last_SYNCH_sent.find(node) == last_SYNCH_sent.end()) {
            last_SYNCH_sent[node] = 1;
        }
        else {
            last_SYNCH_sent[node]++; //saving that we tried to send SYNCH_START
        }
    }

    /*saving when synch start has been sent [to make sure we do so every 5 seconds] */
    latest_SYNCH_START_sending = get_current_timestamp();
    if (any_success) {
        synch_state.synch_sender_state = WAITING_FOR_REQUEST;
        synch_state.waits_from = latest_SYNCH_START_sending;
    }
}


void NetworkNode::receive_SYNC_START(const char* senders_address, uint16_t senders_port, uint8_t* buffer, size_t buffer_size) {
    if (!node_known(senders_address, senders_port)) {
        message_err(buffer, buffer_size);
        return;
    }

    uint64_t t2 = get_current_timestamp();
    last_SYNCH_received[Node(senders_address, senders_port)] = t2; //saving last time of synchronization
    size_t current_pos = 1;

    if (buffer_size < current_pos + sizeof(uint8_t)) {
        message_err(buffer, buffer_size);
        fprintf(stderr, "ERROR not enough bytes for synchronized\n");
        return;
    }

    /*reading synchronized*/
    uint8_t sender_sync_level = buffer[current_pos];
    current_pos += sizeof(sender_sync_level);

    if (buffer_size < current_pos + sizeof(uint64_t)) {
        message_err(buffer, buffer_size); //couldn't read timestamp
        return;
    }

    /*reading timestamp*/
    uint64_t sender_timestamp_net;
    memcpy(&sender_timestamp_net, &buffer[current_pos], sizeof(sender_timestamp_net));
    uint64_t sender_timestamp = be64toh(sender_timestamp_net);

    /* checking if we are currently synchronized with the sender */
    bool is_sync_with_sender = (sync_master_id.get_ip() == senders_address
        && sync_master_id.get_port() == senders_port
        && is_synchronized);

    if (is_sync_with_sender) {
        bool should_sync = (sender_sync_level < synchronized_lvl);
        if (!should_sync) { //received a message with sync level larger than one's own
            message_err(buffer, buffer_size); /*ignoring the message */
            synchronized_lvl = UINT8_MAX;
            is_synchronized = false;
            return;
        }
    }
    else {
        if (sender_sync_level >= UINT8_MAX-1) {
            message_err(buffer, buffer_size); /*ignoring the message*/
            return;
        }
        if (sender_sync_level + 2 > synchronized_lvl) {
            message_err(buffer, buffer_size); /*ignoring the message*/
            return;
        }
    }

    /*checking if the node is already synchronizing with other node */
    if (synch_state.synch_receiver_state == WAITING_FOR_RESPONSE && get_current_timestamp() - synch_state.waits_from < 2*SYNCH_START_SPAN) {
        message_err(buffer, buffer_size); /*ignoring the communicate */
        return;
    }

    /*node is ready to synchronize since the timeout of waiting for response has passed */
    if (synch_state.synch_receiver_state == WAITING_FOR_RESPONSE) {
        synch_state.synch_receiver_state = IDLE;
    }

    /* Saving data about synchronization */
    synch_state.sync_partner_id = senders_address; //who we are trying to synchronize currently [ip]
    synch_state.sync_partner_port = senders_port; //who we are trying to synchronize currently [port]
    synch_state.partner_sync_level = sender_sync_level; //partner's synchronized level
    synch_state.T1 = sender_timestamp; //first timestamp
    synch_state.T2 = t2; //timestamp of receiving the SYNC_START message
    synch_state.synch_receiver_state = WAITING_FOR_RESPONSE; //waiting for t4 which will be received in DELAY_RESPONSE

    /*sending DELAY_REQUEST and saving the time */
    synch_state.T3 = get_current_timestamp();

    synch_state.waits_from = get_current_timestamp(); //to check if timeout didn't pass
    send_DELAY_REQUEST(senders_address, senders_port);
}


void NetworkNode::receive_DELAY_REQUEST(const char* senders_address, uint16_t senders_port, uint8_t* buffer, size_t buffer_size) {
    /*checking if we've sent SYNC_START to that node*/
    if (last_SYNCH_sent.find(Node(senders_address, senders_port)) == last_SYNCH_sent.end()) {
        message_err(buffer, buffer_size); //received message from unknown node
        return;
    }

    last_SYNCH_sent[Node(senders_address, senders_port)]--;
    if (last_SYNCH_sent[Node(senders_address, senders_port)] == 0) {
        last_SYNCH_sent.erase(Node(senders_address, senders_port));
    }

    uint64_t current_time = get_current_timestamp();
    uint64_t time_since_any_message = current_time - synch_state.waits_from;

    if (time_since_any_message > 2*SYNCH_START_SPAN) {
        message_err(buffer, buffer_size); //ignoring message
        if (last_SYNCH_sent.size() == 0) {
            synch_state.synch_sender_state = NONE; //no DELAY_RESPONSES left to receive
        }
        return;
    }

    /*checking if node should be sending SYNCH_START at all */
    if (synch_state.synch_sender_state != WAITING_FOR_REQUEST) {
        message_err(buffer, buffer_size);
        return; //the node didn't start the synchronization process yet received this message
    }
    send_DELAY_RESPONSE(senders_address, senders_port);
}

/*Response to HELLO message. Node receives all the nodes known by the one to whom it
 *sent HELLO*/
void NetworkNode::receive_HELLO_REPLY(const char* senders_address, uint16_t senders_port, uint8_t* buffer, size_t buffer_size) {
    if (count(pendingHELLOS.begin(), pendingHELLOS.end(), Node(senders_address, senders_port)) < 1){
        message_err(buffer, buffer_size);
        return;
    }
    pendingHELLOS.erase(Node(senders_address, senders_port));
    std::vector<Node> received_nodes;
    size_t current_pos = 1;

    if (buffer_size < current_pos + sizeof(uint16_t)) {
        message_err(buffer, buffer_size); //not enough bytes for node count
        return;
    }

    uint16_t received_count_net;
    memcpy(&received_count_net, &buffer[current_pos], sizeof(received_count_net));
    uint16_t received_count = ntohs(received_count_net);
    current_pos += sizeof(received_count_net);

    for (uint32_t i = 0; i < received_count; i++) {
        if (current_pos + sizeof(uint8_t) > buffer_size) {
            message_err(buffer, buffer_size); //buffer too small to read address length
            return;
        }

        uint8_t addr_length = buffer[current_pos];
        current_pos += sizeof(uint8_t);

        if (current_pos + addr_length > buffer_size) {
            message_err(buffer, buffer_size); //buffer too small to read ip
            return;
        }

        struct in_addr ip_addr;
        memcpy(&ip_addr, &buffer[current_pos], addr_length);
        current_pos += addr_length;

        char ip_str[INET_ADDRSTRLEN];

        //check for correct peer address
        if (inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN) == nullptr) {
            message_err(buffer, buffer_size);
            return;
        }

        if (current_pos + sizeof(uint16_t) > buffer_size) {
            message_err(buffer, buffer_size);
            return; //incorrect message format, returning
        }

        uint16_t port_net;
        memcpy(&port_net, &buffer[current_pos], sizeof(port_net));
        current_pos += sizeof(port_net);
        uint16_t port = ntohs(port_net);

        /*checking if we got receiver or sender of this message */
        if ((ip_str == senders_address && port == senders_port) ||
            (ip_str == get_ip() && port == get_port())) {
            message_err(buffer, buffer_size);
            return;
        }
        received_nodes.push_back(Node(ip_str, port));
    }

    /*sending connect requests */
    for (auto& node : received_nodes) {
        bool ok = send_CONNECT(node.get_ip().c_str(), node.get_port());
        if (ok) {
            pendingCONNECTS.insert(Node(node.get_ip().c_str(), node.get_port()));
        }
        else {
            fprintf(stderr, "ERROR error sending CONNECT to %s:%d", node.get_ip().c_str(), node.get_port());
        }
    }
    addNode(senders_address, senders_port); //receiver and sender are now connected
}

void NetworkNode::receive_LEADER(uint8_t* buffer, size_t buffer_size) {
    size_t current_pos = 1;

    if (buffer_size < current_pos + sizeof(uint8_t)) {
        message_err(buffer, buffer_size); //not enough bytes for synchronized
        return;
    }

    /*reading synchronized */
    uint8_t synchronized = buffer[current_pos];

    if (synchronized == 0) {
        synchronized_lvl = 0;
        latest_SYNCH_START_sending = get_current_timestamp() + SYNCH_START_SPAN - 2000; //after 2 secs the synch start will be sent
    }
    else if (synchronized == 255) {
        if (synchronized_lvl == 0) {
            synchronized_lvl = 255; //with this lvl the node won't send any new SYNC_STARTS
            is_synchronized = false;
        }
        else {
            message_err(buffer, buffer_size); //not-a-leader received this message
        }
    }
    else {
        message_err(buffer, buffer_size); //incorrect synchronized value
    }
}

/*Base function to receive message and determine the type.*/
void NetworkNode::receive_message() {
    uint8_t buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, MAX_BUFFER_SIZE);

    struct sockaddr_in sender_addr;
    socklen_t addr_len = (socklen_t) sizeof(sender_addr);

    ssize_t bytes_received = recvfrom(my_fd, &buffer, sizeof(buffer), 0,
                                      (sockaddr*)&sender_addr, &addr_len);

    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        else {
            fprintf(stderr, "ERROR Recvfrom error");
            return;
        }
    }
    else if (bytes_received == 0) {
        fprintf(stderr, "ERROR Empty datagram received\n");
        return;
    }

    uint8_t message = buffer[0];
    const char *senders = inet_ntoa(sender_addr.sin_addr);
    uint16_t senders_port = ntohs(sender_addr.sin_port);

    switch (message) {
        case HELLO:
            //fprintf(stderr, "received HELLO from %s %d \n", senders, senders_port);
            send_HELLO_REPLY(senders, senders_port, buffer, bytes_received);
            break;
        case HELLO_REPLY:
            //fprintf(stderr, "received HELLO_REPLY from %s %d \n", senders, senders_port);
            receive_HELLO_REPLY(senders, senders_port, buffer, bytes_received);
            break;
        case CONNECT:
            //fprintf(stderr, "received CONNECT from %s %d \n", senders, senders_port);
            if (addNode(senders, senders_port)) {
                send_ACK_CONNECT(senders, senders_port);
            }
            break;
        case ACK_CONNECT:
            //fprintf(stderr, "received ACK_CONNECT from %s %d \n", senders, senders_port);
            if (count(pendingCONNECTS.begin(), pendingCONNECTS.end(), Node(senders, senders_port)) < 1) {
                message_err(buffer, bytes_received);
                break;
            }
            pendingCONNECTS.erase(Node(senders, senders_port));
            addNode(senders, senders_port);
            break;
        case GET_TIME:
            //fprintf(stderr, "received GET_TIME from %s %d \n", senders, senders_port);
            send_TIME(senders, senders_port);
            break;
        case SYNC_START:
            //fprintf(stderr, "received SYNC_START from %s %d \n", senders, senders_port);
            receive_SYNC_START(senders, senders_port, buffer, bytes_received);
            break;
        case DELAY_REQUEST:
            //fprintf(stderr, "received DELAY_REQUEST from %s %d \n", senders, senders_port);
            receive_DELAY_REQUEST(senders, senders_port, buffer, bytes_received);
            break;
        case DELAY_RESPONSE:
            //fprintf(stderr, "received DELAY_RESPONSE from %s %d \n", senders, senders_port);
            receive_DELAY_RESPONSE(senders, senders_port, buffer, bytes_received);
            break;
        case LEADER:
            //fprintf(stderr, "received LEADER from %s %d \n", senders, senders_port);
            receive_LEADER(buffer, bytes_received);
            break;
        default:
            message_err(buffer, bytes_received);
    }
}
