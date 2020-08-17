//========= Copyright (c) 2012, AGHL.RU, All rights reserved. ============
//
// Purpose: Network communications.
//
//=============================================================================

#ifndef NET_H
#define NET_H
#include <cstdint>

/**
 * A cross-platform type abstraction over socket descriptor.
 */
using NetSocket = uintptr_t;

int NetSendReceiveUdp(const char *addr, int port, const char *sendbuf, int len, char *recvbuf, int size);
int NetSendReceiveUdp(unsigned long sin_addr, int sin_port, const char *sendbuf, int len, char *recvbuf, int size);
int NetSendUdp(unsigned long sin_addr, int sin_port, const char *sendbuf, int len, NetSocket *socket_out);
int NetReceiveUdp(unsigned long sin_addr, int sin_port, char *recvbuf, int size, NetSocket socket_in);
void NetClearSocket(NetSocket s);
void NetCloseSocket(NetSocket s);

char *NetGetRuleValueFromBuffer(const char *buffer, int len, const char *cvar);

#endif
