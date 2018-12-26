#include "pch.hpp"
#include <assert.h>
#include <limits>
#include "base/Endian.hpp"
#include "base/Convertors.hpp"
#include "base/SHA256.hpp"
#include "base/StringUtils.hpp"
#include "BeamSolution.hpp"

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

static void CompressArray(
	const unsigned char* in, size_t in_len,
	unsigned char* out, size_t out_len,
	size_t bit_len, size_t byte_pad)
{
	assert(bit_len >= 8);
	assert(8 * sizeof(uint32_t) >= bit_len);

	size_t in_width{ (bit_len + 7) / 8 + byte_pad };
	assert(out_len == (bit_len*in_len / in_width + 7) / 8);

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
			if (j < in_len) {
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
			else {
				acc_value <<= 8 - acc_bits;
				acc_bits += 8 - acc_bits;;
			}
		}

		acc_bits -= 8;
		out[i] = (acc_value >> acc_bits) & 0xFF;
	}
}
#if 0
#ifdef _WIN32
inline uint32_t htobe32(uint32_t x)
{
	return (((x & 0xff000000U) >> 24) | ((x & 0x00ff0000U) >> 8) |
		((x & 0x0000ff00U) << 8) | ((x & 0x000000ffU) << 24));
}
#endif // WIN32
#endif
// Big-endian so that lexicographic array comparison is equivalent to integer comparison
inline void EhIndexToArray(const uint32_t i, unsigned char* array)
{
	static_assert(sizeof(uint32_t) == 4, "sizeof(uint32_t) != 4");
	uint32_t bei = htobe32(i);
	memcpy(array, &bei, sizeof(uint32_t));
}

// Helper function that compresses the solution from 32 unsigned integers (128 bytes) to 104 bytes
static void GetMinimalFromIndices(std::vector<uint32_t> indices, size_t cBitLen, std::vector<unsigned char> &aSolution)
{
	assert(((cBitLen + 1) + 7) / 8 <= sizeof(uint32_t));
	size_t lenIndices{ indices.size() * sizeof(uint32_t) };
	size_t minLen{ (cBitLen + 1)*lenIndices / (8 * sizeof(uint32_t)) };
	size_t bytePad{ sizeof(uint32_t) - ((cBitLen + 1) + 7) / 8 };
	std::vector<unsigned char> array(lenIndices);
	for (size_t i = 0; i < indices.size(); i++) {
		EhIndexToArray(indices[i], array.data() + (i * sizeof(uint32_t)));
	}
	aSolution.resize(minLen);
	CompressArray(array.data(), lenIndices, aSolution.data(), minLen, cBitLen + 1, bytePad);
}

void BeamSolution::Add(const std::vector<uint32_t> &aIndexVector, size_t aCbitlen)
{
	// get the compressed representation of the solution and check against target
	GetMinimalFromIndices(aIndexVector, aCbitlen, _solution);

	CSHA256 sha;
	sha.Write(&_solution.front(), _solution.size());
	sha.Finalize(_hash.GetBytes());
}

const BigInteger & BeamSolution::GetHash()
{
	return _hash;
}

bool BeamSolution::CheckSolution(const std::vector<uint32_t> &aIndexVector, size_t aCbitlen)
{
	return false;
}

bool BeamSolution::ProofOfWork()
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

BeamSolution::BeamSolution(const std::string &aDeviceId, const BeamWork &aWork, core::Statistics *aStatistics)
	: _deviceId(aDeviceId)
	, _statistics(aStatistics)
	, _hash(32, false)
{
	_workId = aWork._id;
	_nonce = aWork._nonce;
}

const std::string & BeamSolution::GetDeviceId() const
{
	return _deviceId;
}

const std::string & BeamSolution::GetWorkId() const
{
	return _workId;
}

void BeamSolution::PrintTime(std::string &aDst) const
{
//	binToString(aDst, _header + 4 * 25, 4, false, false);
}

void BeamSolution::PrintNonce(std::string &aDst, bool aBigEndian) const
{
	binToString(aDst, (unsigned char*)&_nonce, 8, aBigEndian, false);
}

void BeamSolution::PrintSolution(std::string &aDst) const
{
	binToString(aDst, &_solution.front(), _solution.size(), true, true);
}

void BeamSolution::OnAccepted(unsigned aDuration)
{
	if (_statistics) {
		_statistics->accepted++;
	}
}

void BeamSolution::OnRejected(unsigned aDuration, const std::string &aReason)
{
	if (_statistics) {
		_statistics->rejected++;
	}
}
