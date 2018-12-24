#include "pch.hpp"
#include "TcpServerSocket.hpp"
#include "base/Logging.hpp"
#ifndef _WIN32
#include <poll.h>
#endif

bool TcpServerSocket::Bind(const char *aInterface, uint16_t aPort, DWORD aRecvTimeout, DWORD aSendTimeout)
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
	}

	//setup address structure
	if (inet_addr(aInterface) == -1)
	{
		struct hostent *he;
		struct in_addr **addr_list;

		//resolve the hostname, its not an ip address
		if ((he = gethostbyname(aInterface)) == NULL)
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
		server.sin_addr.s_addr = inet_addr(aInterface);
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(aPort);

	//Connect to remote server
	if (::bind(_socket, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
#ifdef _WIN32
		_error = WSAGetLastError();
#else
		_error = errno;
#endif
		Close();
		return false;
	}

	return true;
}

bool TcpServerSocket::Listen()
{
	if (::listen(_socket, SOMAXCONN) < 0) {
#ifdef _WIN32
		_error = WSAGetLastError();
#else
		_error = errno;
#endif
		Close();
		return false;
	}
	_isConnected = true;
	return true;
}

SOCKET TcpServerSocket::Accept()
{
	if (INVALID_SOCKET != _socket) {
		SOCKET s = ::accept(_socket, NULL, NULL);
		if (INVALID_SOCKET == s) {
			Close();
		}
		else {
			return s;
		}
	}
	return INVALID_SOCKET;
}

void TcpServerSocket::Close()
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

