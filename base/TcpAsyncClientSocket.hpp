#pragma	once

#ifdef _WIN32
#include "Event.hpp"
#endif
#include "TcpClientSocket.hpp"

class TcpAsyncClientSocket : public TcpClientSocket
{
#ifdef _WIN32
	OVERLAPPED		_overlap;
	win32::Event	_event;
	win32::Event	_cancelEvent;

public:
	TcpAsyncClientSocket() : _event(FALSE, TRUE), _cancelEvent(FALSE, TRUE)
	{
		ZeroMemory(&_overlap, sizeof(_overlap));
		_overlap.hEvent = _event.GetNativeHandle();
	}

	void Close() override;
	int Recv(char* buf, size_t maxLen, DWORD aRecvTimeout = INFINITE) override;
#endif
};

