#include "pch.hpp"
#include "EquihashWork.hpp"
#include "base/Endian.hpp"
#include "base/Convertors.hpp"

//  0	version
//  1	prevhash
//  2	|
//  3	|
//  4	|
//  5	|
//  6	|
//  7	|
//  8	|
//  9	merkle root
// 10	|
// 11	|
// 12	|
// 13	|
// 14	|
// 15	|
// 16	|
// 17	reserve
// 18	|
// 19	|
// 20	|
// 21	|
// 22	|
// 23	|
// 24	|
// 25	time
// 26	nbits
// 27	nonce
// 28	|
// 29	|
// 30	|
// 31	|
// 32	|
// 33	|
// 34	|

// 0007fff800000000000000000000000000000000000000000000000000000000 is stratum diff 32
// 003fffc000000000000000000000000000000000000000000000000000000000 is stratum diff 4
// 00ffff0000000000000000000000000000000000000000000000000000000000 is stratum diff 1
static double TargetToDiff(uint32_t* target)
{
	unsigned char* tgt = (unsigned char*)target;
	uint64_t m =
		(uint64_t)tgt[30] << 24 |
		(uint64_t)tgt[29] << 16 |
		(uint64_t)tgt[28] << 8 |
		(uint64_t)tgt[27] << 0;

	if (!m) {
		return 0.;
	}
	else {
		return (double)0xffff0000UL / m;
	}
}

bool EquihashWork::Target::FromDiff(double aDiff)
{
	uint64_t m;
	int k;
	for (k = 6; k > 0 && aDiff > 1.0; k--) {
		aDiff /= 4294967296.0;
	}
	m = (uint64_t)(4294901760.0 / aDiff);
	if (m == 0 && k == 6) {
		memset(&_value.front(), 0xff, 32);
	}
	else {
		uint32_t *target = reinterpret_cast<uint32_t *>(&_value.front());
		memset(target, 0, 32);
		target[k + 1] = (uint32_t)(m >> 8);
		target[k + 2] = (uint32_t)(m >> 40);
		//memset(target, 0xff, 6*sizeof(uint32_t));
		for (k = 0; k < 28 && ((uint8_t*)target)[k] == 0; k++) {
			((uint8_t*)target)[k] = 0xff;
		}
	}
	
	return true;
}

bool EquihashWork::Target::FromBits(uint32_t aBits)
{
	return true;
}

EquihashWork::EquihashWork()
{
	_data.resize(4 + 32 + 32 + 32 + 4 + 4 + 32);
}

EquihashWork::EquihashWork(
	uint32_t aVersion,
	const std::string &aPrevHash,
	const std::string &aMerkleRoot,
	uint32_t aTime,
	const std::string &aBits,
	const std::string &aNonce
)
{
	_data.resize(4 + 32 + 32 + 32 + 4 + 4 + 32);

	unsigned char *cp = _data.data();

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
	memset(cp, 0, 32);
	cp += 32;
	*reinterpret_cast<uint32_t*>(cp) = aTime;
	cp += 4;
	if (4 != HexToBin(aBits, cp, 4)) {
		return;
	}
	cp += 4;
	if (aNonce.empty()) {
		memset(cp, 0, 32);
	}
	else {
		if (32 != HexToBin(aNonce, cp, 32)) {
			return;
		}
	}
}

double EquihashWork::GetNetworkDiff() const
{
#if 0
	//KMD bits: "1e 015971",
	//KMD target: "00 00 015971000000000000000000000000000000000000000000000000000000",
	//KMD bits: "1d 686aaf",
	//KMD target: "00 0000 686aaf0000000000000000000000000000000000000000000000000000",
	uint32_t bits = (_nbits & 0xffffff);
	int16_t shift = (bswap_32(_nbits) & 0xff);
	shift = (31 - shift) * 8; // 8 bits shift for 0x1e, 16 for 0x1d
	uint64_t tgt64 = bswap_32(bits);
	tgt64 = tgt64 << shift;
	uint8_t net_target[32] = { 0 };
	for (int b = 0; b < 8; b++) {
		net_target[31 - b] = ((uint8_t*)&tgt64)[b];
	}
	return TargetToDiff((uint32_t*)net_target);
#endif
	return 0;
}

unsigned char * EquihashWork::GetNonce()
{
	return &_data.front() + 4 * 27;
}

uint32_t EquihashWork::GetNonceSize() const
{
	return 32;
}

core::Work::Ref EquihashWork::CreateForThread(uint32_t aThreadId) const
{
	if (EquihashWork::Ref work = new EquihashWork(*this)) {
		reinterpret_cast<uint32_t*>(work->GetData())[33] = aThreadId;
		work->_increment = reinterpret_cast<uint32_t*>(work->GetNonce()) + (_extraNonceSize + 3)/sizeof(uint32_t);
		return work.get();
	}
	return core::Work::Ref();
}

void EquihashWork::Increment(unsigned aCount)
{
	(*_increment) += aCount;
}

unsigned char * EquihashWork::GetData()
{
	return &_data.front();
}

const unsigned char * EquihashWork::GetData() const
{
	return &_data.front();
}

uint32_t EquihashWork::GetDataSize() const
{
	return (uint32_t)_data.size();
}

