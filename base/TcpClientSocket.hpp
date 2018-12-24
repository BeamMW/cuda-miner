#pragma	once

#ifdef _WIN32
#include <mswsock.h>
#else
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int SOCKET;
#define	INVALID_SOCKET	-1
#define SD_BOTH	SHUT_RDWR
#endif

class TcpClientSocket
{
protected:
	SOCKET	_socket = INVALID_SOCKET;
	bool	_isConnected = false;

public:
	TcpClientSocket() {}

	TcpClientSocket(SOCKET aSock) : _socket(aSock), _isConnected(INVALID_SOCKET != aSock) {}

	virtual ~TcpClientSocket();

	virtual bool Connect(const char *aHost, uint16_t aPort, DWORD aRecvTimeout = 0, DWORD aSendTimeout = 0);
	virtual void Close();
	virtual int Recv(char* buf, size_t maxLen, DWORD aRecvTimeout = INFINITE);
	virtual int Send(const char* buf, size_t len);

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

