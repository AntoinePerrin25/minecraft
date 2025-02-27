#include "rnet.h"
#include <enet/enet.h>
#include <string.h>

struct rnetPeer {
    ENetHost* host;
    ENetPeer* peer;
    bool isServer;
    ENetPeer* lastEventPeer;
};

struct rnetTargetPeer {
    // On garde la structure opaque mais on l'implémente comme ENetPeer
    ENetHost* host;
    // ...autres champs d'ENetPeer...
};

// Fonction auxiliaire pour la conversion
static inline ENetPeer* targetPeerToENetPeer(rnetTargetPeer* target) {
    return (ENetPeer*)target;
}

static inline rnetTargetPeer* enetPeerToTargetPeer(ENetPeer* peer) {
    return (rnetTargetPeer*)peer;
}

bool rnetInit(void) {
    return enet_initialize() == 0;
}

void rnetShutdown(void) {
    enet_deinitialize();
}

rnetPeer* rnetHost(uint16_t port) {
    ENetAddress address = {0};
    address.host = ENET_HOST_ANY;
    address.port = port;
    
    ENetHost* host = enet_host_create(&address, 32, 2, 0, 0);
    if (!host) return NULL;
    
    rnetPeer* peer = malloc(sizeof(rnetPeer));
    peer->host = host;
    peer->peer = NULL;
    peer->isServer = true;
    peer->lastEventPeer = NULL;
    return peer;
}

rnetPeer* rnetConnect(const char* address, uint16_t port) {
    ENetHost* host = enet_host_create(NULL, 1, 2, 0, 0);
    if (!host) return NULL;
    
    ENetAddress addr = {0};
    enet_address_set_host(&addr, address);
    addr.port = port;
    
    ENetPeer* peer = enet_host_connect(host, &addr, 2, 0);
    if (!peer) {
        enet_host_destroy(host);
        return NULL;
    }
    
    // Attendre que la connexion soit établie
    ENetEvent event;
    if (enet_host_service(host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        rnetPeer* rpeer = malloc(sizeof(rnetPeer));
        rpeer->host = host;
        rpeer->peer = peer;
        rpeer->isServer = false;
        rpeer->lastEventPeer = NULL;
        return rpeer;
    }
    
    // Si on arrive ici, la connexion a échoué
    enet_peer_reset(peer);
    enet_host_destroy(host);
    return NULL;
}

void rnetClose(rnetPeer* peer) {
    if (!peer) return;
    if (!peer->isServer && peer->peer) {
        enet_peer_disconnect(peer->peer, 0);
    }
    enet_host_destroy(peer->host);
    free(peer);
}

bool rnetSend(rnetPeer* peer, const void* data, size_t size, int flags) {
    if (!peer || !peer->peer) return false;
    
    ENetPacket* packet = enet_packet_create(data, size, 
        flags == RNET_RELIABLE ? ENET_PACKET_FLAG_RELIABLE : 0);
    
    return enet_peer_send(peer->peer, 0, packet) == 0;
}

bool rnetBroadcast(rnetPeer* peer, const void* data, size_t size, int flags) {
    if (!peer || !peer->host) return false;
    
    ENetPacket* packet = enet_packet_create(data, size,
        flags == RNET_RELIABLE ? ENET_PACKET_FLAG_RELIABLE : 0);
    
    enet_host_broadcast(peer->host, 0, packet);
    return true;
}

bool rnetReceive(rnetPeer* peer, rnetPacket* packet) {
    if (!peer || !peer->host) return false;
    
    ENetEvent event;
    if (enet_host_service(peer->host, &event, 0) > 0) {
        peer->lastEventPeer = event.peer;  // Stocker le peer qui a envoyé l'événement
        
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                packet->data = NULL;  // Signaler un événement de connexion
                packet->size = 0;
                return true;
            
            case ENET_EVENT_TYPE_RECEIVE:
                packet->data = malloc(event.packet->dataLength);
                memcpy(packet->data, event.packet->data, event.packet->dataLength);
                packet->size = event.packet->dataLength;
                enet_packet_destroy(event.packet);
                return true;
            
            case ENET_EVENT_TYPE_DISCONNECT:
                if (!peer->isServer) {
                    peer->peer = NULL;
                }
                packet->data = NULL;  // Signaler un événement de déconnexion
                packet->size = 0;
                return true;
            
            default:
                return false;
        }
    }
    return false;
}

bool rnetSendToPeer(rnetPeer* peer, rnetTargetPeer* targetPeer, const void* data, size_t size, int flags) {
    if (!peer || !targetPeer) return false;
    
    ENetPacket* packet = enet_packet_create(data, size, 
        flags == RNET_RELIABLE ? ENET_PACKET_FLAG_RELIABLE : 0);
    
    return enet_peer_send(targetPeerToENetPeer(targetPeer), 0, packet) == 0;
}

rnetTargetPeer* rnetGetLastEventPeer(rnetPeer* peer) {
    return peer ? enetPeerToTargetPeer(peer->lastEventPeer) : NULL;
}

void rnetFreePacket(rnetPacket* packet) {
    if (packet && packet->data) {
        free(packet->data);
        packet->data = NULL;
        packet->size = 0;
    }
}