//correct error handling!!!
//make sure to check if data sent via udp is complete!!!m


//todo: check for correct number of arguments.


extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
}

#include <iostream>
#include "err.h"
#include "utils.h"
#include "InputParser.h"
#include "NetworkNode.h"
#include <bits/stdc++.h>

#define ANY 0;
#define NOT_GIVEN 0; //address may be 0 for correct input, it may but not must be a problem


using namespace std;

//if sth here fails we call syserr because these are some basic functionalities
NetworkNode parseArguments(int argc, char *argv[], InputParser input) {
    const string &b = input.getCmdOption("-b");
    const string &p = input.getCmdOption("-p");
    uint32_t bind_address;
    uint32_t port;

    if (!input.cmdOptionExists("-b")) {bind_address = INADDR_ANY;}
    else {
        bind_address = inet_addr(b.c_str());
        if (bind_address == INADDR_NONE) {
            fatal("Invalid bind address: %s", b.c_str());
        }
    }

    if (!input.cmdOptionExists("-p")) {port = ANY;}
    else {
        try {
            port = std::stoull(p, nullptr, 10);
            if (port > 65535) {
                fatal("Invalid port: %s. Enter valid port number in range 0 to 65535", p.c_str());
            }
        } catch (const std::exception& e) {
            fatal("Invalid port: %s", p.c_str());
        }
    }

    //creating socket with given address and port
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; //ipv4
    server_address.sin_addr.s_addr = bind_address;
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (sockaddr *) &server_address, sizeof(server_address)) < 0) {
        syserr("bind");
    }

    if (port == 0) {
        socklen_t len = sizeof(server_address);
        if (getsockname(socket_fd, (sockaddr*)&server_address, &len) < 0) {
            fatal("Cannot get socket name: %s", strerror(errno));
        }
        port = ntohs(server_address.sin_port); //from net order to host order
    }

    printf("Listening on port %u\n", port);

    return (NetworkNode(b.c_str(), 4500, socket_fd));
}


//todo: what if argument is given more than once? prob fatal, choosing one out of a few seems absurd
//not sure why you're not using preexisting functions from utils xdd
int main(int argc, char *argv[]) {
    InputParser input(argc, argv);
    string a = input.getCmdOption("-a");
    string r = input.getCmdOption("-r");

    //checking if args are given in a correct way
    if ((!a.empty() && r.empty()) || (a.empty() && !r.empty())) {
        fatal("-a and -r arguments must given be both");
    }

    //this node has some basic data so host, port and fd
    NetworkNode node = parseArguments(argc, argv, input);
    std::string peer_address;
    uint16_t peer_port = NOT_GIVEN;

    if (input.cmdOptionExists("-r")) {
        peer_address = a;
        //validating
        struct in_addr addr_struct;
        if (inet_pton(AF_INET, peer_address.c_str(), &addr_struct) != 1) {
            fatal("Invalid peer address: %s", peer_address.c_str());
        }

        printf("peer address: %s\n", peer_address.c_str());

        //peer port
        peer_port = read_port(r.c_str());
        node.send_HELLO(peer_address.c_str(), peer_port);
    }

    node.addNode("196.88.75.173", 5666);
    node.addNode("196.88.75.173", 9999);
    if (node.get_ip() == "10.1.1.153") node.addNode("10.1.1.153", 4000);
    int i=0;
    while (i<5) {
        node.receive_message();
        i++;
        node.report_nodes();
        fprintf(stderr, "%lu\n", node.get_current_timestamp());
    }

    return 0;
}



