/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

#include "../../../addon_bindings/c_cpp/addon_api.h"
#include <array>
#include <cstdint>

struct Addon;
struct PlayerSession;

class AddonNetwork {
public:
	enum class Result { Unhandled, Incomplete, Handled, Invalid };

	bool Register(Addon& _owner, uint8_t _id, bp_packet_decode_fn _decode,
	              bp_packet_receive_fn _receive, void* _userData) noexcept;
	bool Unregister(Addon& _owner, uint8_t _id) noexcept;
	void Remove(Addon& _owner) noexcept;
	Result Dispatch(uint8_t _id, PlayerSession& _session) const;

private:
	struct Entry {
		Addon* owner = nullptr;
		bp_packet_decode_fn decode = nullptr;
		bp_packet_receive_fn receive = nullptr;
		void* userData = nullptr;
	};

	std::array<Entry, 256> entries{};
};

bp_network_api MakeAddonNetworkAPI();
