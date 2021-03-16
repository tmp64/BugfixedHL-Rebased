/***
*
*	Copyright (c) 2012, AGHL.RU, All rights reserved.
*
*	Purpose: Network communications.
*
****/

#include <unistd.h>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <ctime>
#include "net.h"

// UnixSock uses int
#define SocketConvert(s) (s)

int NetSendReceiveUdp(const char *addr, int port, const char *sendbuf, int len, char *recvbuf, int size)
{
	unsigned long ulAddr = inet_addr(addr);
	return NetSendReceiveUdp(ulAddr, htons((unsigned short)port), sendbuf, len, recvbuf, size);
}

int NetSendReceiveUdp(unsigned long sin_addr, int sin_port, const char *sendbuf, int len, char *recvbuf, int size)
{
	// Create a socket for sending data
	int SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in addr, fromaddr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = sin_addr;
	addr.sin_port = sin_port;

	// Send request
	sendto(SendSocket, sendbuf, len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	time_t timeout;
	timeout = time(NULL) + 3;

	// Receive response
	int res;
	unsigned long nonzero = 1;
	ioctl(SendSocket, FIONBIO, &nonzero);
	int maxfd;
	fd_set rfd;
	fd_set efd;
	struct timeval tv;
	while (true)
	{
		// Do select on socket
		FD_ZERO(&rfd);
		FD_ZERO(&efd);
		FD_SET(SendSocket, &rfd);
		FD_SET(SendSocket, &efd);
		maxfd = SendSocket;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		res = select(maxfd + 1, &rfd, NULL, &efd, &tv);
		// Check for error on socket
		if (res == -1 || FD_ISSET(SendSocket, &efd))
		{
			break;
		}
		time_t current;
		current = time(NULL);
		if (res == 0)
		{
			if (current >= timeout)
				break;
			continue;
		}
		// Get what we received
		socklen_t fromaddrlen = sizeof(struct sockaddr_in);
		res = recvfrom(SendSocket, recvbuf, size, 0, (struct sockaddr *)&fromaddr, &fromaddrlen);
		// Check for error on socket
		if (res == -1)
		{
			break;
		}
		// Check address from which data came
		if (res >= 0 && addr.sin_addr.s_addr == fromaddr.sin_addr.s_addr && addr.sin_port == fromaddr.sin_port)
		{
			close(SendSocket);
			return res;
		}
		if (current >= timeout)
			break;
	}
	close(SendSocket);
	return -1;
}

int NetSendUdp(unsigned long sin_addr, int sin_port, const char *sendbuf, int len, NetSocket *s)
{
	// Create or get a socket for sending data
	int s1;
	if (s && *s)
		s1 = SocketConvert(*s);
	else
		s1 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = sin_addr;
	addr.sin_port = sin_port;

	// Send buffer
	int res = sendto(s1, sendbuf, len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	// Return socket if send was succeded
	if (res != -1 && s)
		*s = SocketConvert(s1);
	else
		close(s1);

	return res;
}

int NetReceiveUdp(unsigned long sin_addr, int sin_port, char *recvbuf, int size, NetSocket ns)
{
	if (!ns)
		return -1;

	int s = SocketConvert(ns);
	struct sockaddr_in addr, fromaddr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = sin_addr;
	addr.sin_port = sin_port;

	// Try to receive response
	int res;
	unsigned long nonzero = 1;
	ioctl(s, FIONBIO, &nonzero);
	int maxfd;
	fd_set rfd;
	fd_set efd;
	struct timeval tv;

	// Do select on socket
	FD_ZERO(&rfd);
	FD_ZERO(&efd);
	FD_SET(s, &rfd);
	FD_SET(s, &efd);
	maxfd = s;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	res = select(maxfd + 1, &rfd, NULL, &efd, &tv);
	// Check for error on socket
	if (res == -1 || FD_ISSET(s, &efd))
		return -1;
	// Return if nothing to receive
	if (res == 0)
		return 0;
	// Get what we had received
	socklen_t fromaddrlen = sizeof(struct sockaddr_in);
	res = recvfrom(s, recvbuf, size, 0, (struct sockaddr *)&fromaddr, &fromaddrlen);
	// Check for error on socket
	if (res == -1)
		return -1;
	// Check address from which data came
	if (res >= 0 && (addr.sin_addr.s_addr == 0 && addr.sin_port == 0) || (addr.sin_addr.s_addr == fromaddr.sin_addr.s_addr && addr.sin_port == fromaddr.sin_port))
	{
		return res;
	}
	return 0;
}

void NetClearSocket(NetSocket s)
{
	if (!s)
		return;
	char buffer[2048];
	while (NetReceiveUdp(0, 0, buffer, sizeof(buffer), SocketConvert(s)) > 0)
		;
}

void NetCloseSocket(NetSocket s)
{
	if (!s)
		return;
	close(SocketConvert(s));
}

char *NetGetRuleValueFromBuffer(const char *buffer, int len, const char *cvar)
{
	// Check response header
	if (len < 6 || (*(unsigned int *)buffer != 0xFFFFFFFF || buffer[4] != 'E'))
		return NULL;
	// Search for a cvar
	char *current = (char *)buffer + 4;
	char *end = (char *)buffer + len - strlen(cvar);
	while (current < end)
	{
		char *pcvar = (char *)cvar;
		while (*current == *pcvar && *pcvar != 0)
		{
			current++;
			pcvar++;
		}
		if (*pcvar == 0)
		{
			// Found
			return current + 1;
		}
		current++;
	}
	return NULL;
}
