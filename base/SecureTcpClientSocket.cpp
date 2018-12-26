#include "pch.hpp"
#include "SecureTcpClientSocket.hpp"
#include "base/Logging.hpp"
#ifndef _WIN32
#include <poll.h>
#endif

static bool gSslIsInited = false;
static bool gSslIsEnabled = false;

SecureTcpClientSocket::SecureTcpClientSocket()
{
	if (!gSslIsInited) {
		if (1 == SSL_library_init()) {
			gSslIsEnabled = true;
		}
		gSslIsInited = true;
	}
	if (gSslIsEnabled) {
		const SSL_METHOD *method;

		OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
		SSL_load_error_strings();   /* Bring in and register error messages */
		method = SSLv23_client_method();  /* Create new client-method instance */
		_sslCtx = SSL_CTX_new(method);   /* Create new context */
		if (nullptr == _sslCtx) {
			char buffer[128];
			ERR_error_string_n(ERR_get_error(), buffer, sizeof(buffer));
			LOG(Error) << "Create SSL Context error: " << buffer;
		}
	}
}

SecureTcpClientSocket::~SecureTcpClientSocket()
{
	Close();
}

bool SecureTcpClientSocket::Connect(const char *aHost, uint16_t aPort, DWORD aRecvTimeout, DWORD aSendTimeout)
{
	if (TcpClientSocket::Connect(aHost, aPort, aRecvTimeout, aSendTimeout)) {
		if (_sslCtx) {
			_ssl = SSL_new(_sslCtx);
			if (nullptr != _ssl) {
				SSL_set_fd(_ssl, _socket);
				if (-1 != SSL_connect(_ssl)) {
					LOG(Trace) << "Connected with " << SSL_get_cipher(_ssl) << " encryption";
					return true;
				} else {
					char buffer[128];
					ERR_error_string_n(ERR_get_error(), buffer, sizeof(buffer));
					LOG(Error) << "SSL connect error: " << buffer;
				}
			}
		}
		Close();
		return false;
	}
	return false;
}

void SecureTcpClientSocket::Close()
{
	if (INVALID_SOCKET != _socket) {
		if (_ssl) {
			SSL_free(_ssl);
			_ssl = nullptr;
		}

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
		if (_sslCtx) {
			SSL_CTX_free(_sslCtx);
			_sslCtx = nullptr;
		}
	}
}

int SecureTcpClientSocket::Recv(char* buf, size_t maxLen, DWORD aRecvTimeout)
{
	size_t readLen = 0;

	if (!_isConnected) {
		return -1;
	}

	int ret = SSL_read(_ssl, buf, (int)maxLen);

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

int SecureTcpClientSocket::Send(const char* buf, size_t len)
{
	size_t writtenLen = 0;

	if (!_isConnected) {
		return -1;
	}

	while (len > 0) {
		int ret = SSL_write(_ssl, buf + writtenLen, (int)len);
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
