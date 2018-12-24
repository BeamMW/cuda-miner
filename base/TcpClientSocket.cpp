#include "pch.hpp"
#include "TcpClientSocket.hpp"
#include "base/Logging.hpp"
#ifndef _WIN32
#include <poll.h>
#endif

TcpClientSocket::~TcpClientSocket()
{
	Close();
}

bool TcpClientSocket::Connect(const char *aHost, uint16_t aPort, DWORD aRecvTimeout, DWORD aSendTimeout)
{
	struct sockaddr_in server;

	//create socket if it is not already created
	if (INVALID_SOCKET == _socket)
	{
		//Create socket
		_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _socket)
		{
			return false;
		}
		if (aRecvTimeout) {
			SetRecvTimeout(aRecvTimeout);
		}
		if (aSendTimeout) {
			SetSendTimeout(aSendTimeout);
		}
#ifndef _WIN32
		// On linux with SO_REUSEADDR, bind will get the port if the previous
		// socket is closed (even if it is still in TIME_WAIT) but fail if
		// another program has it open - which is what we want
		int optval = 1;
		// If it doesn't work, we don't really care - just show a debug message
		if (0 != setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (void *)(&optval), sizeof(optval))) {
			LOG(Error) << "API setsockopt SO_REUSEADDR failed (ignored): %s", strerror(errno);
		}
#endif
	}

	//setup address structure
	if (inet_addr(aHost) == (unsigned)-1)
	{
		struct hostent *he;
		struct in_addr **addr_list;

		//resolve the hostname, its not an ip address
		if ((he = gethostbyname(aHost)) == NULL)
		{
			return false;
		}

		//Cast the h_addr_list to in_addr , since h_addr_list also has the ip address in long format only
		addr_list = (struct in_addr **) he->h_addr_list;

		for (int i = 0; addr_list[i] != NULL; i++)
		{
			//strcpy(ip , inet_ntoa(*addr_list[i]) );
			server.sin_addr = *addr_list[i];
			break;
		}
	}
	//plain ip address
	else
	{
		server.sin_addr.s_addr = inet_addr(aHost);
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(aPort);

	//Connect to remote server
	if (::connect(_socket, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
#if 0
		int err = WSAGetLastError();
#endif
		return false;
	}

	_isConnected = true;

	return true;
}

void TcpClientSocket::Close()
{
	if (INVALID_SOCKET != _socket) {
#ifdef _WIN32
		setsockopt(_socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
#endif
		shutdown(_socket, SD_BOTH);
#ifdef _WIN32
		closesocket(_socket);
#else
		::close((int)_socket);
#endif
		_socket = INVALID_SOCKET;
		_isConnected = false;
	}
}

int TcpClientSocket::Recv(char* buf, size_t maxLen, DWORD aRecvTimeout)
{
	size_t readLen = 0;

	if (!_isConnected) {
		return -1;
	}
#ifdef _WIN32
	int ret = ::recv(_socket, buf, (int)maxLen, 0);
#else
	struct pollfd fd;
	fd.fd = _socket;
	fd.events = POLLIN;
	int ret = poll(&fd, 1, aRecvTimeout);
	if (ret < 0) {
		return -1;
	}
	if (0 == ret) {
		return -2;
	}
	ret = ::recv(_socket, buf, (int)maxLen, 0);
#endif
	if (ret > 0) {
		readLen += ret;
	}
	else if (ret == 0) {
		Close();
	}
	else {
		return ret;
	}

	return (int)readLen;
}

int TcpClientSocket::Send(const char* buf, size_t len)
{
	size_t writtenLen = 0;

	if (!_isConnected) {
		return -1;
	}

	while (len > 0) {
		int ret = ::send(_socket, buf + writtenLen, (int)len, 0);
		if (ret > 0) {
			writtenLen += ret;
			len -= ret;
		}
		else if (ret == 0) {
			Close();
			return ret;
		}
		else {
			return ret;
		}
	}

	return (int)writtenLen;
}
