#pragma	once

#include "TcpClientSocket.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>

class SecureTcpClientSocket : public TcpClientSocket
{
protected:
	SSL_CTX		*_sslCtx = nullptr;
	SSL			*_ssl = nullptr;

public:
	SecureTcpClientSocket();

	~SecureTcpClientSocket() override;

	bool Connect(const char *aHost, uint16_t aPort, DWORD aRecvTimeout = 0, DWORD aSendTimeout = 0) override;
	void Close() override;
	int Recv(char* buf, size_t maxLen, DWORD aRecvTimeout = INFINITE) override;
	int Send(const char* buf, size_t len) override;

protected:
	bool InitSecureContext();
};

