#include "pch.hpp"
#include "TcpAsyncTransport.hpp"

using namespace core;

TcpAsyncTransport::TcpAsyncTransport(const std::string &aServer, int aPort)
	: _server(aServer)
	, _port(aPort)
{
}

TcpAsyncTransport::~TcpAsyncTransport()
{
	Close();
}

bool TcpAsyncTransport::IsReady() const
{
	return _socket.IsConnected();
}

bool TcpAsyncTransport::Open()
{
	return _socket.Connect(_server.c_str(), _port);
}

void TcpAsyncTransport::Close()
{
	_socket.Close();
}

int TcpAsyncTransport::GetLine(std::string &aLine, DWORD aTimeout)
{
	if (_inBuffer) {
		if (char *tail = (char*)memchr(_head, '\n', _inBuffer)) {
			aLine.assign(_head, tail);
			_inBuffer -= int(tail - _head) + 1;
			_head = tail + 1;
			return 0;
		}
		else {
			aLine.assign(_head, _inBuffer);
			_inBuffer = 0;
		}
	}
	while (true) {
		int result = _socket.Recv(_buffer, sizeof(_buffer), aTimeout);
		if (result <= 0) {
			return result;
		}
		_inBuffer = result;
		if (char *tail = (char*)memchr(_buffer, '\n', _inBuffer)) {
			result = int(tail - _buffer);
			aLine.append(_buffer, result);
			_inBuffer -= result + 1;
			_head = tail + 1;
			return 0;
		}
		aLine.append(_buffer, _inBuffer);
		_inBuffer = 0;
	}
}

int TcpAsyncTransport::PutLine(const std::string &aLine)
{
	return _socket.Send(aLine.c_str(), aLine.length());
}
