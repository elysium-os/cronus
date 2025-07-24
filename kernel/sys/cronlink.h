#pragma once

#include <stdint.h>

#ifdef __ENV_DEVELOPMENT
#define CRONLINK_PACKET_START(SIGNATURE) cronlink_packet_start(SIGNATURE)
#define CRONLINK_PACKET_STRING(STR) cronlink_packet_write_string(STR)
#define CRONLINK_PACKET_UINT(N) cronlink_packet_write_uint(N)
#define CRONLINK_PACKET_END() cronlink_packet_end()
#else
#define CRONLINK_PACKET_START(SIGNATURE)
#define CRONLINK_PACKET_STRING(STR)
#define CRONLINK_PACKET_UINT(N)
#define CRONLINK_PACKET_END()
#endif

/// Start cronlink packet.
void cronlink_packet_start(uint8_t type);

/// Write a string into the open packet.
void cronlink_packet_write_string(const char *str);

/// Write an unsigned integer into the open packet.
void cronlink_packet_write_uint(uint64_t n);

/// End cronlink packet.
void cronlink_packet_end();
