#pragma	once

#include "base/TcpClientSocket.hpp"

class TcpServerSocket
{
	SOCKET	_socket = INVALID_SOCKET;
	bool	_isConnected = false;
	int		_error = 0;

public:
	TcpServerSocket() {}

	~TcpServerSocket()
	{
		Close();
	}

	bool Bind(const char *aInterface, uint16_t aPort, DWORD aRecvTimeout = 0, DWORD aSendTimeout = 0);
	bool Listen();
	SOCKET Accept();
	void Close();

	bool IsConnected() const { return _isConnected; }
	bool IsInvalid() const { return INVALID_SOCKET == _socket; }

	bool SetRecvTimeout(DWORD aMilliseconds)
	{
		if (INVALID_SOCKET != _socket) {
#ifdef _WIN32
			return 0 == setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&aMilliseconds, sizeof(aMilliseconds));
#else
			struct timeval tv;
			tv.tv_sec = aMilliseconds / 1000;
			tv.tv_usec = 0;
			return 0 == setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
#endif
		}
		return false;
	}

	bool SetSendTimeout(DWORD aMilliseconds)
	{
		if (INVALID_SOCKET != _socket) {
#ifdef _WIN32
			return 0 == setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&aMilliseconds, sizeof(aMilliseconds));
#else
			struct timeval tv;
			tv.tv_sec = aMilliseconds / 1000;
			tv.tv_usec = 0;
			return 0 == setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
#endif
		}
		return false;
	}
};

