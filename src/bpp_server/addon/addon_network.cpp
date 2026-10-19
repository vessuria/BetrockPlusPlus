/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "addon_network.h"
#include "addon.h"
#include "packet_ids.h"
#include "player_conn/player_session.h"
#include "server.h"

namespace {

// TODO: Theres gotta be a better way
// Grabbed from https://pixelbrush.dev/beta-wiki/networking/packets/
bool IsVanillaPacket(uint8_t _id) noexcept {
	switch (static_cast<PacketId>(_id)) {
	case PacketId::KeepAlive:
	case PacketId::Login:
	case PacketId::PreLogin:
	case PacketId::ChatMessage:
	case PacketId::SetTime:
	case PacketId::SetEquipment:
	case PacketId::SetSpawnPosition:
	case PacketId::InteractWithEntity:
	case PacketId::SetHealth:
	case PacketId::Respawn:
	case PacketId::PlayerMovement:
	case PacketId::PlayerPosition:
	case PacketId::PlayerRotation:
	case PacketId::PlayerPositionAndRotation:
	case PacketId::MineBlock:
	case PacketId::PlaceBlock:
	case PacketId::SetHotbarSlot:
	case PacketId::InteractWithBlock:
	case PacketId::Animation:
	case PacketId::PlayerAction:
	case PacketId::SpawnPlayer:
	case PacketId::SpawnItem:
	case PacketId::CollectItem:
	case PacketId::SpawnObject:
	case PacketId::SpawnMob:
	case PacketId::SpawnPainting:
	case PacketId::PlayerInput:
	case PacketId::EntityVelocity:
	case PacketId::DespawnEntity:
	case PacketId::EntityMovement:
	case PacketId::EntityPosition:
	case PacketId::EntityRotation:
	case PacketId::EntityPositionAndRotation:
	case PacketId::TeleportEntity:
	case PacketId::EntityEvent:
	case PacketId::AddPassenger:
	case PacketId::EntityMetadata:
	case PacketId::SetChunkVisibility:
	case PacketId::Chunk:
	case PacketId::SetMultipleBlocks:
	case PacketId::SetBlock:
	case PacketId::BlockEvent:
	case PacketId::Explosion:
	case PacketId::WorldEvent:
	case PacketId::GameEvent:
	case PacketId::LightningBolt:
	case PacketId::OpenContainer:
	case PacketId::CloseContainer:
	case PacketId::ClickSlot:
	case PacketId::SetSlot:
	case PacketId::FillContainer:
	case PacketId::ContainerData:
	case PacketId::ContainerTransaction:
	case PacketId::UpdateSign:
	case PacketId::ItemData:
	case PacketId::IncrementStatistic:
	case PacketId::Disconnect:
		return true;
	default:
		return false;
	}
}

bool RegisterPacket(const bp_api* _api, uint8_t _id, bp_packet_decode_fn _decode,
                    bp_packet_receive_fn _receive, void* _userData) noexcept {
	try {
		if (!_api || !_api->internal)
			return false;
		auto* addon = static_cast<Addon*>(_api->internal);
		if (!addon->server)
			return false;
		return addon->server->GetAddonManager().Network().Register(*addon, _id, _decode, _receive, _userData);
	} catch (...) {
		return false;
	}
}

bool UnregisterPacket(const bp_api* _api, uint8_t _id) noexcept {
	try {
		if (!_api || !_api->internal)
			return false;
		auto* addon = static_cast<Addon*>(_api->internal);
		if (!addon->server)
			return false;
		return addon->server->GetAddonManager().Network().Unregister(*addon, _id);
	} catch (...) {
		return false;
	}
}

bool SendPacket(bp_player* _player, uint8_t _id, const uint8_t* _data, size_t _size) noexcept {
	try {
		if (!_player || !_player->session || (!_data && _size != 0) || _size > BP_MAX_PACKET_PAYLOAD)
			return false;
		auto& session = *_player->session;
		if (session.connState != ConnectionState::Playing || !session.stream.IsConnected())
			return false;
		session.stream.WritePacket(_id, _data, _size);
		return true;
	} catch (...) {
		return false;
	}
}

} // namespace

bool AddonNetwork::Register(Addon& _owner, uint8_t _id, bp_packet_decode_fn _decode,
                            bp_packet_receive_fn _receive, void* _userData) noexcept {
	if (!_decode || !_receive || IsVanillaPacket(_id) || entries[_id].owner)
		return false;
	entries[_id] = Entry{ &_owner, _decode, _receive, _userData };
	return true;
}

bool AddonNetwork::Unregister(Addon& _owner, uint8_t _id) noexcept {
	if (entries[_id].owner != &_owner)
		return false;
	entries[_id] = {};
	return true;
}

void AddonNetwork::Remove(Addon& _owner) noexcept {
	for (auto& entry : entries) {
		if (entry.owner == &_owner)
			entry = {};
	}
}

AddonNetwork::Result AddonNetwork::Dispatch(uint8_t _id, PlayerSession& _session) const {
	const Entry entry = entries[_id];
	if (!entry.owner) return Result::Unhandled;

	const auto bytes = _session.stream.GetReadableBytes();
	size_t payloadSize = 0;

	const auto result = entry.decode(&entry.owner->api, bytes.data(), bytes.size(), &payloadSize, entry.userData);
	switch (result) {
	case BP_PACKET_INCOMPLETE:
		return bytes.size() >= BP_MAX_PACKET_PAYLOAD ? Result::Invalid : Result::Incomplete;
	case BP_PACKET_READY:
		if (payloadSize > bytes.size() || payloadSize > BP_MAX_PACKET_PAYLOAD)
			return Result::Invalid;
		break;
	case BP_PACKET_INVALID:
	default:
		return Result::Invalid;
	}

	_session.stream.SkipBytes(payloadSize);
	const bp_packet_event event{ &_session.apiPlayer, _id, bytes.data(), payloadSize };
	entry.receive(&entry.owner->api, &event, entry.userData);
	return Result::Handled;
}

bp_network_api MakeAddonNetworkAPI() {
	return bp_network_api{
		.registerPacket = RegisterPacket,
		.unregisterPacket = UnregisterPacket,
		.sendPacket = SendPacket,
	};
}
