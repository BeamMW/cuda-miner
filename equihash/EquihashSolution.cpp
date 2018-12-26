#include "pch.hpp"
#include <assert.h>
#include <limits>
#include "base/Endian.hpp"
#include "base/Convertors.hpp"
#include "base/SHA256.hpp"
#include "base/StringUtils.hpp"
#include "EquihashSolution.hpp"
#if 0
#include "EquihashSolver.hpp"
#endif

#undef max

typedef uint32_t eh_index;

static int WriteCompactSize(unsigned char *aDst, uint64_t nSize)
{
	if (nSize < 253) {
		*aDst = (unsigned char)nSize;
		return 1;
	}
	else if (nSize <= std::numeric_limits<unsigned short>::max()) {
		*aDst++ = 253;
		*reinterpret_cast<unsigned short*>(aDst) = (unsigned short)nSize;
		return 3;
	}
	else if (nSize <= std::numeric_limits<unsigned int>::max()) {
		*aDst++ = 254;
		*reinterpret_cast<unsigned int*>(aDst) = (unsigned int)nSize;
		return 5;
	}
	else {
		*aDst++ = 255;
		*reinterpret_cast<uint64_t*>(aDst) = nSize;
		return 9;
	}
}

static void CompressArray(const unsigned char* in, size_t in_len, unsigned char* out, size_t out_len, size_t bit_len, size_t byte_pad)
{
	assert(bit_len >= 8);
	assert(8 * sizeof(uint32_t) >= 7 + bit_len);

	size_t in_width{ (bit_len + 7) / 8 + byte_pad };
	assert(out_len == bit_len*in_len / (8 * in_width));

	uint32_t bit_len_mask{ ((uint32_t)1 << bit_len) - 1 };

	// The acc_bits least-significant bits of acc_value represent a bit sequence
	// in big-endian order.
	size_t acc_bits = 0;
	uint32_t acc_value = 0;

	size_t j = 0;
	for (size_t i = 0; i < out_len; i++) {
		// When we have fewer than 8 bits left in the accumulator, read the next
		// input element.
		if (acc_bits < 8) {
			acc_value = acc_value << bit_len;
			for (size_t x = byte_pad; x < in_width; x++) {
				acc_value = acc_value | (
					(
						// Apply bit_len_mask across byte boundaries
						in[j + x] & ((bit_len_mask >> (8 * (in_width - x - 1))) & 0xFF)
						) << (8 * (in_width - x - 1))); // Big-endian
			}
			j += in_width;
			acc_bits += bit_len;
		}

		acc_bits -= 8;
		out[i] = (acc_value >> acc_bits) & 0xFF;
	}
}

static void GetMinimalFromIndices(std::vector<eh_index> indices, size_t cBitLen, std::vector<unsigned char> &aRet)
{
//	BOOST_STATIC_ASSERT(sizeof(eh_index) == 4);
	assert(((cBitLen + 1) + 7) / 8 <= sizeof(eh_index));
	size_t lenIndices{ indices.size() * sizeof(eh_index) };
	size_t minLen{ (cBitLen + 1)*lenIndices / (8 * sizeof(eh_index)) };
	size_t bytePad{ sizeof(eh_index) - ((cBitLen + 1) + 7) / 8 };
	std::vector<unsigned char> array(lenIndices);
	for (int i = 0; i < indices.size(); i++) {
		*reinterpret_cast<eh_index*>(array.data() + (i * sizeof(eh_index))) = htobe32(indices[i]);
	}
	aRet.resize(minLen);
	CompressArray(array.data(), lenIndices, aRet.data(), minLen, cBitLen + 1, bytePad);
}

bool EquihashSolution::Add(const std::vector<uint32_t> &aIndexVector, size_t aCbitlen)
{
	std::vector<unsigned char> nSolution;
	GetMinimalFromIndices(aIndexVector, aCbitlen, nSolution);

	if (1344 == nSolution.size()) {
		unsigned char *cp = _header + 140;
		if (3 != WriteCompactSize(cp, 1344)) {
			return false;
		}
		memcpy(cp + 3, nSolution.data(), 1344);

		CSHA256 sha;
		unsigned char buf[sha.OUTPUT_SIZE];
		sha.Write(_header, sizeof(_header));
		sha.Finalize(buf);
		sha.Reset().Write(buf, sha.OUTPUT_SIZE).Finalize(_hash.GetBytes());
		return true;
	}

	return false;
}

const BigInteger & EquihashSolution::GetHash()
{
	return _hash;
}

bool EquihashSolution::CheckSolution(const std::vector<uint32_t> &aIndexVector, size_t aCbitlen)
{
	std::vector<unsigned char> nSolution;
	GetMinimalFromIndices(aIndexVector, aCbitlen, nSolution);
	if (1344 == nSolution.size()) {
		return 0 == memcmp(_header + 140 + 3, nSolution.data(), 1344);
	}
	return false;
}

bool EquihashSolution::ProofOfWork()
{
#if 0
	EquihashSolver eq(1);
	eq.setnonce((char*)_header, 140, nullptr, 0);
	eq.digit0(0);
	eq.xfull = eq.bfull = eq.hfull = 0;
	u32 r = 1;

	for (; r < WK; r++) {
		r & 1 ? eq.digitodd(r, 0) : eq.digiteven(r, 0);
		eq.xfull = eq.bfull = eq.hfull = 0;
	}

	eq.digitK(0);

	for (unsigned s = 0; s < eq.nsols; s++)
	{
		std::vector<uint32_t> index_vector(PROOFSIZE);
		for (u32 i = 0; i < PROOFSIZE; i++) {
			index_vector[i] = eq.sols[s][i];
		}

		if (CheckSolution(index_vector, DIGITBITS)) {
			return true;
		}
	}
#endif
	return false;
}

EquihashSolution::EquihashSolution(
	const std::string &aDeviceId,
	uint32_t aVersion,
	const std::string &aPrevHash,
	const std::string &aMerkleRoot,
	uint32_t aTime,
	const std::string &aBits,
	const std::string &aNonce,
	const std::string &aSolution
)
	: _deviceId(aDeviceId)
	, _hash(32, false)
{
	unsigned char *cp = _header;

	*reinterpret_cast<uint32_t*>(cp) = aVersion;
	cp += 4;
	if (32 != HexToBin(aPrevHash, cp, 32)) {
		return;
	}
	cp += 32;
	if (32 != HexToBin(aMerkleRoot, cp, 32)) {
		return;
	}
	cp += 32;
	ZeroMemory(cp, 32);
	cp += 32;
	*reinterpret_cast<uint32_t*>(cp) = aTime;
	cp += 4;
	if (4 != HexToBin(aBits, cp, 4)) {
		return;
	}
	cp += 4;
	if (32 != HexToBin(aNonce, cp, 32)) {
		return;
	}
	cp += 32;
	if (3 != WriteCompactSize(cp, 1344)) {
		return;
	}
	cp += 3;
	if (1344 != HexToBin(aSolution, cp, 1344)) {
		return;
	}

	CSHA256 sha;
	unsigned char buf[sha.OUTPUT_SIZE];
	sha.Write(_header, sizeof(_header));
	sha.Finalize(buf);
	sha.Reset().Write(buf, sha.OUTPUT_SIZE).Finalize(_hash.GetBytes());
}

EquihashSolution::EquihashSolution(const std::string &aDeviceId, const EquihashWork &aWork, core::Statistics *aStatistics)
	: _deviceId(aDeviceId)
	, _statistics(aStatistics)
	, _hash(32, false)
{
	_workId = aWork._id;
	_extraNonceSize = aWork.GetExtraNonceSize();
	if (140 == aWork.GetDataSize()) {
		memcpy(_header, aWork.GetData(), 140);
	}
}

const std::string & EquihashSolution::GetDeviceId() const
{
	return _deviceId;
}

const std::string & EquihashSolution::GetWorkId() const
{
	return _workId;
}

void EquihashSolution::PrintTime(std::string &aDst) const
{
	binToString(aDst, _header + 4 * 25, 4, false, false);
}

void EquihashSolution::PrintNonce(std::string &aDst, bool aBigEndian) const
{
	binToString(aDst, _header + 4 * 27 + _extraNonceSize, 32 - _extraNonceSize, aBigEndian, false);
}

void EquihashSolution::PrintSolution(std::string &aDst) const
{
	binToString(aDst, _header + 140, 3 + 1344, false, false);
}

void EquihashSolution::OnAccepted(unsigned aDuration)
{
	if (_statistics) {
		_statistics->accepted++;
	}
}

void EquihashSolution::OnRejected(unsigned aDuration, const std::string &aReason)
{
	if (_statistics) {
		_statistics->rejected++;
	}
}
