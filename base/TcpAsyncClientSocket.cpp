#include "pch.hpp"
#include "TcpAsyncClientSocket.hpp"
#ifdef _WIN32
void TcpAsyncClientSocket::Close()
{
	if (INVALID_SOCKET != _socket) {
		_cancelEvent.SetEvent();
		TcpClientSocket::Close();
		_cancelEvent.ResetEvent();
	}
}

int TcpAsyncClientSocket::Recv(char* buf, size_t maxLen, DWORD aRecvTimeout)
{
	if (!_isConnected) {
		return -1;
	}

	DWORD recvBytes, flags = 0;
	WSABUF recvBuf;
	recvBuf.buf = buf;
	recvBuf.len = (ULONG)maxLen;

	_event.ResetEvent();

	if (0 == WSARecv(_socket, &recvBuf, 1, &recvBytes, &flags, &_overlap, nullptr)) {
		return recvBytes;
	}

	if (WSA_IO_PENDING != WSAGetLastError()) {
		Close();
		return -1;
	}

	HANDLE handles[] = { _event.GetNativeHandle(), _cancelEvent.GetNativeHandle() };

	DWORD dwStatus = WaitForMultipleObjects(2, handles, FALSE, aRecvTimeout);

	if (WAIT_TIMEOUT == dwStatus) {
		return -2;
	}

	if (WAIT_OBJECT_0 != dwStatus) {
		return -1;
	}

	if (!WSAGetOverlappedResult(_socket, &_overlap, &recvBytes, FALSE, &flags)) {
//		WSAGetLastError();
		return -1;
	}

	return recvBytes;
}
#endif
