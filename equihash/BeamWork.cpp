#include "pch.hpp"
#include "BeamWork.hpp"
#include "base/Endian.hpp"
#include "base/Convertors.hpp"
#include "base/Utils.hpp"

std::atomic<uint64_t> BeamWork::sNonce(GenRandomU64());

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

BeamWork::BeamWork()
{
}

BeamWork::BeamWork(const std::string &aBlockHeader, const std::string &aNonce) : _input(32, true)
{
	_input.Import(aBlockHeader, true);
	HexToBin(aNonce, (unsigned char*)&_nonce, 8);
}

unsigned char * BeamWork::GetNonce()
{
	return (unsigned char *)&_nonce;
}

uint32_t BeamWork::GetNonceSize() const
{
	return 8;
}

core::Work::Ref BeamWork::CreateForThread(uint32_t aThreadId) const
{
	if (BeamWork::Ref work = new BeamWork(*this)) {
		work->_nonce = sNonce++;
		return work.get();
	}
	return core::Work::Ref();
}

void BeamWork::Increment(unsigned aCount)
{
	_nonce = sNonce++;
}

unsigned char * BeamWork::GetData()
{
	return _input.GetBytes();
}

const unsigned char * BeamWork::GetData() const
{
	return _input.GetBytes();
}

uint32_t BeamWork::GetDataSize() const
{
	return _input.GetBytesLength();
}

