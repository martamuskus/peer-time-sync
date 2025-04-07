#ifndef READ_WRITE_H
#define READ_WRITE_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

ssize_t readn(int fd, void *vptr, size_t n);
ssize_t writen(int fd, const void *vptr, size_t n);

typedef enum {
    SYNC_NONE,
    SYNC_WAITING_FOR_REQUEST,
    SYNC_WAITING_FOR_RESPONSE
} SynchState;

// Synchronization context for tracking current sync process
struct {
    uint8_t sync_partner_id;         // ID of the node we're synchronizing with
    uint8_t partner_sync_level;      // Synchronization level of the partner
    uint32_t T1;                     // Timestamp from SYNC_START
    uint32_t T2;                     // Local timestamp when SYNC_START received
    uint32_t T3;                     // Local timestamp when DELAY_REQUEST sent
    uint32_t T4;                     // Timestamp from DELAY_RESPONSE
    SynchState state;                 // Current state in the sync process
    uint64_t last_sync_time;         // Time of last successful sync
} typedef SynchContext;

#endif //READ_WRITE_H
