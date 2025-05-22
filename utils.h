#ifndef UTILS_H
#define UTILS_H

uint16_t read_port(char const *string);
sockaddr_in get_server_address(char const *host, uint16_t port);

#endif
