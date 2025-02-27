#ifndef RNET_H
#define RNET_H

// Définir WINDOWS_LEAN_AND_MEAN et autres définitions nécessaires
#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct rnetPeer rnetPeer;
typedef struct rnetTargetPeer rnetTargetPeer;

typedef struct {
    void* data;
    size_t size;
} rnetPacket;

#define RNET_RELIABLE 1
#define RNET_UNRELIABLE 0

bool rnetInit(void);
void rnetShutdown(void);
rnetPeer* rnetHost(uint16_t port);
rnetPeer* rnetConnect(const char* address, uint16_t port);
void rnetClose(rnetPeer* peer);
bool rnetSend(rnetPeer* peer, const void* data, size_t size, int flags);
bool rnetBroadcast(rnetPeer* peer, const void* data, size_t size, int flags);
bool rnetReceive(rnetPeer* peer, rnetPacket* packet);
void rnetFreePacket(rnetPacket* packet);
bool rnetSendToPeer(rnetPeer* peer, rnetTargetPeer* targetPeer, const void* data, size_t size, int flags);
rnetTargetPeer* rnetGetLastEventPeer(rnetPeer* peer);

#endif // RNET_H