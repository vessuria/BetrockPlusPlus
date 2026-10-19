/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "addon_api.h"

// LLM Disclosure
static unsigned registrations_ok;

static bp_packet_decode_result length_packet(const bp_api* api, const uint8_t* data,
                                             size_t available, size_t* payload_size,
                                             void* user_data) {
    (void)api;
    (void)user_data;
    if (available < 2)
        return BP_PACKET_INCOMPLETE;
    size_t length = ((size_t)data[0] << 8) | data[1];
    if (length > 4096)
        return BP_PACKET_INVALID;
    *payload_size = length + 2;
    return available >= *payload_size ? BP_PACKET_READY : BP_PACKET_INCOMPLETE;
}

static bp_packet_decode_result empty_packet(const bp_api* api, const uint8_t* data,
                                            size_t available, size_t* payload_size,
                                            void* user_data) {
    (void)api;
    (void)data;
    (void)available;
    (void)user_data;
    *payload_size = 0;
    return BP_PACKET_READY;
}

static bp_packet_decode_result bad_ready(const bp_api* api, const uint8_t* data,
                                         size_t available, size_t* payload_size,
                                         void* user_data) {
    (void)api;
    (void)user_data;
    if (!available)
        return BP_PACKET_INCOMPLETE;
    *payload_size = data[0] ? available + 1 : (size_t)BP_MAX_PACKET_PAYLOAD + 1;
    return BP_PACKET_READY;
}

static bp_packet_decode_result never_complete(const bp_api* api, const uint8_t* data,
                                              size_t available, size_t* payload_size,
                                              void* user_data) {
    (void)api;
    (void)data;
    (void)available;
    (void)payload_size;
    (void)user_data;
    return BP_PACKET_INCOMPLETE;
}

static void receive_packet(const bp_api* api, const bp_packet_event* event, void* user_data) {
    if (user_data != &registrations_ok || !registrations_ok) {
        api->player.sendMessage(event->player, "network fixture registration failure");
        return;
    }
    if (api->network.sendPacket(event->player, 0xE0, NULL, 1) ||
        api->network.sendPacket(event->player, 0xE0, event->data, (size_t)BP_MAX_PACKET_PAYLOAD + 1) ||
        api->network.sendPacket(NULL, 0xE0, NULL, 0)) {
        api->player.sendMessage(event->player, "network fixture invalid send accepted");
        return;
    }
    if (event->id == 0xE0) {
        uint8_t* copy = malloc(event->size);
        if (!copy) {
            api->player.sendMessage(event->player, "network fixture allocation failure");
            return;
        }
        memcpy(copy, event->data, event->size);
        bool sent = api->network.sendPacket(event->player, event->id, copy, event->size);
        /* sendPacket must own a synchronous copy, not this temporary storage. */
        memset(copy, 0xA5, event->size);
        free(copy);
        if (!sent)
            api->player.sendMessage(event->player, "network fixture send failure");
    } else if (event->id == 0xE1 || event->id == 0xE2) {
        if (event->size != 0 ||
            (event->id == 0xE2 && !api->network.unregisterPacket(api, event->id)) ||
            !api->network.sendPacket(event->player, event->id, NULL, 0)) {
            api->player.sendMessage(event->player, "network fixture empty/unregister failure");
        }
    } else {
        api->player.sendMessage(event->player, "network fixture invalid packet delivered");
    }
}

#ifdef COLLISION_ADDON
static void player_join(const bp_api* api, const bp_player_join_event* event) {
    bool registered = api->network.registerPacket(api, 0xE0, length_packet, receive_packet,
                                                 &registrations_ok);
    bool removed = api->network.unregisterPacket(api, 0xE0);
    api->player.sendMessage(event->player, !registered && !removed
                           ? "network collision rejected" : "network collision failure");
}
#else
static void addon_load(const bp_api* api, const bp_addon_load* event) {
    (void)event;
    registrations_ok = api->version == ADDON_API_VERSION;
    registrations_ok &= api->network.registerPacket(api, 0xE0, length_packet, receive_packet, &registrations_ok);
    registrations_ok &= api->network.registerPacket(api, 0xE1, empty_packet, receive_packet, &registrations_ok);
    registrations_ok &= api->network.registerPacket(api, 0xE2, empty_packet, receive_packet, &registrations_ok);
    registrations_ok &= api->network.registerPacket(api, 0xE3, length_packet, receive_packet, &registrations_ok);
    registrations_ok &= api->network.registerPacket(api, 0xE4, bad_ready, receive_packet, &registrations_ok);
    registrations_ok &= api->network.registerPacket(api, 0xE5, never_complete, receive_packet, &registrations_ok);
    registrations_ok &= !api->network.registerPacket(api, 0xE0, length_packet, receive_packet, &registrations_ok);
    registrations_ok &= !api->network.registerPacket(api, 0xE6, NULL, receive_packet, NULL);
    registrations_ok &= !api->network.registerPacket(api, 0xE6, length_packet, NULL, NULL);
    const uint8_t vanilla[] = {0x00, 0x01, 0x02, 0x03, 0x0D, 0xFF};
    for (size_t i = 0; i < sizeof(vanilla); ++i)
        registrations_ok &= !api->network.registerPacket(api, vanilla[i], empty_packet, receive_packet, NULL);
}
#endif

bp_addon_info bp_addon(const bp_api* api) {
    (void)api;
    bp_addon_info info = {0};
#ifdef COLLISION_ADDON
    info.id = "network_collision";
    info.name = "Network collision fixture";
    info.events.playerJoin = player_join;
#else
    info.id = "network_fixture";
    info.name = "Network framing fixture";
    info.events.addonLoad = addon_load;
#endif
    info.version = "1";
    return info;
} // End LLM Disclosure
