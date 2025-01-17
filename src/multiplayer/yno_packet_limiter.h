#ifndef EP_YNO_PACKET_LIMITER_H
#define EP_YNO_PACKET_LIMITER_H

#include "connection_monitor.h"
#include "../game_clock.h"

#include <set>

namespace YNO {

class PacketLimiter : public ConnectionMonitor {
public:
	PacketLimiter() : receive_count(0) {}
	Action OnReceive(std::string_view name, const Multiplayer::S2CPacket& p) override;
protected:
	Game_Clock::time_point last_received_time;
	static const std::set<std::string_view> whitelist;
	size_t receive_count;
};

}

#endif

