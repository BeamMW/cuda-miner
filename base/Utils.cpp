#include "pch.hpp"
#include "Utils.hpp"

#ifdef WIN32
#    pragma comment (lib, "Bcrypt.lib")
#else // WIN32
#    include <unistd.h>
#    include <fcntl.h>
#endif // WIN32

void GenRandom(void* pData, uint32_t nSize)
{
#ifdef _WIN32
	if (S_OK != BCryptGenRandom(NULL, (PUCHAR)pData, nSize, BCRYPT_USE_SYSTEM_PREFERRED_RNG)) {
		throw std::runtime_error("BCryptGenRandom FAILS");
	}
#else
	bool bRet = false;

	int fp = open("/dev/urandom", O_RDONLY);
	if (fp >= 0) {
		if (read(fp, pData, nSize) == nSize) {
			bRet = true;
		}
		close(fp);
	}
	if (!bRet) {
		throw std::runtime_error("Read /dev/urandom FAILS");
	}
#endif // WIN32
}

uint64_t GenRandomU64()
{
	uint64_t value;
	GenRandom(&value, sizeof(value));
	return value;
}
