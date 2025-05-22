#pragma once

constexpr uint8_t HELLO = 1;
constexpr uint8_t HELLO_REPLY = 2;
constexpr uint8_t CONNECT = 3;
constexpr uint8_t ACK_CONNECT = 4;

constexpr uint8_t SYNC_START = 11;
constexpr uint8_t DELAY_REQUEST = 12;
constexpr uint8_t DELAY_RESPONSE = 13;

constexpr uint8_t LEADER = 21;

constexpr uint8_t GET_TIME = 31;
constexpr uint8_t TIME = 32;

constexpr uint16_t MAX_BUFFER_SIZE = UINT16_MAX; //maximal size of one datagram
constexpr uint16_t MAX_NODE_COUNT_TO_SEND = 9358; //maximal count of nodes for package not to exceed size of one datagram

constexpr uint64_t SYNCH_START_SPAN = 5000;