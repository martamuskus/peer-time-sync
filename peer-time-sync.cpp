extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
}

#include <iostream>
#include "err.h"
#include "utils.h"
#include "InputParser.h"
#include "NetworkNode.h"
#include <bits/stdc++.h>

using namespace std;

/*Parsing bind and port arguments to return a valid node. */
NetworkNode parseArguments(const InputParser* input) {
    const std::string& b = input->getCmdOption("-b");
    const std::string& p = input->getCmdOption("-p");

    struct in_addr bind_address_struct;
    uint32_t port;

    //handling bind address
    if (!input->cmdOptionExists("-b")) {
        bind_address_struct.s_addr = INADDR_ANY;
    }
    else {
        if (inet_pton(AF_INET, b.c_str(), &bind_address_struct) != 1) {
            fatal("ERROR Invalid bind address: %s", b.c_str());
        }
    }

    //handling port
    if (!input->cmdOptionExists("-p")) {
        port = 0;
    } else {
        try {
            port = std::stoull(p, nullptr, 10);
            if (port > 65535) {
                fatal("ERROR Invalid port: %s. Enter valid port number in range 0 to 65535", p.c_str());
            }
        }
        catch (const std::exception& e) {
            fatal("ERROR Invalid port: %s", p.c_str());
        }
    }

    //creating a socket
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("ERROR Cannot create a socket");
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr = bind_address_struct;
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (sockaddr*)&server_address, sizeof(server_address)) < 0) {
        syserr("ERROR Bind err. Couldn't bind socket to given address");
    }

    //getting actual port number if the arg given was 0
    if (port == 0) {
        socklen_t len = sizeof(server_address);
        if (getsockname(socket_fd, (sockaddr*)&server_address, &len) < 0) {
            fatal("ERROR Cannot get socket name: %s", strerror(errno));
        }
        port = ntohs(server_address.sin_port);
    }
    return NetworkNode(b.c_str(), port, socket_fd);
}


int main(int argc, char *argv[]) {
    InputParser input(argc, argv);

    if (input.checkForMultipleOptions()) fatal("ERROR Argument given more than once");
    if (input.hasUnexpectedArgs()) fatal("ERROR Unexpected arguments given");

    string a = input.getCmdOption("-a");
    string r = input.getCmdOption("-r");

    //checking if args are given in a correct way
    if ((a.empty() && !r.empty()) || (r.empty() && !a.empty())) {
        fatal("ERROR -a and -r arguments must be given both");
    }

    NetworkNode node = parseArguments(&input);
    std::string peer_address;
    uint16_t peer_port;

    if (input.cmdOptionExists("-r")) {
        peer_address = a;
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET; //ipv4
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo(peer_address.c_str(), nullptr, &hints, &res) > 0) {
            fatal("ERROR Invalid peer address: %s", peer_address.c_str());
        }
        freeaddrinfo(res);

        //peer port
        peer_port = read_port(r.c_str());
        node.send_HELLO(peer_address.c_str(), peer_port);
    }

    while (true) {
        node.timeout_check();
        node.receive_message();
        node.send_SYNC_START();
    }
    return 0;
}



