/***
*
*	Copyright (c) 2012, AGHL.RU, All rights reserved.
*
*	Purpose: Network communications.
*
****/

#include "net.h"

void WinsockInit()
{
}

int NetSendReceiveUdp(const char *addr, int port, const char *sendbuf, int len, char *recvbuf, int size)
{
	return -1;
}

int NetSendReceiveUdp(unsigned long sin_addr, int sin_port, const char *sendbuf, int len, char *recvbuf, int size)
{
	return -1;
}

int NetSendUdp(unsigned long sin_addr, int sin_port, const char *sendbuf, int len, NetSocket *s)
{
	return -1;
}

int NetReceiveUdp(unsigned long sin_addr, int sin_port, char *recvbuf, int size, NetSocket s)
{
	return -1;
}

void NetClearSocket(NetSocket s)
{
}

void NetCloseSocket(NetSocket s)
{
}

char *NetGetRuleValueFromBuffer(const char *buffer, int len, const char *cvar)
{
	return nullptr;
}
