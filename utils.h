#ifndef UTILS_H
#define UTILS_H

uint16_t read_port(char const *string);
sockaddr_in get_server_address(char const *host, uint16_t port);

struct communicate {
    uint8_t message; //typ komunikatu
    uint16_t count;//liczba znanych węzłów;
    uint8_t peer_address_length; //liczba oktetów w polu peer_address;
    char* peer_address; //peer_address_length oktetów, adres IP węzła w formacie jak w nagłówku IP; //max 255*oktet
    uint16_t peer_port; //numer portu, na którym nasłuchuje węzeł;
    uint64_t timestamp; //czas, wartość zegara;
    uint8_t synchronized; //poziom synchronizacji węzła.
};

#endif
