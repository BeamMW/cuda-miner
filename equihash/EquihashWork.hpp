#pragma	once

#include "core/Work.hpp"

class EquihashWork : public core::Work
{
public:
	typedef Reference<EquihashWork> Ref;

	class Target : public BigInteger
	{
	public:
		Target() { _value.resize(32); }

		bool FromDiff(double aDiff);
		bool FromBits(uint32_t aBits);
	};

public:
	EquihashWork();
	EquihashWork(
		uint32_t aVersion,
		const std::string &aPrevHash,
		const std::string &aMerkleRoot,
		uint32_t aTime,
		const std::string &aBits,
		const std::string &aNonce = std::string()
	);

	unsigned char * GetNonce() override;
	uint32_t GetNonceSize() const override;

	core::Work::Ref CreateForThread(uint32_t aThreadId) const override;
	void Increment(unsigned aCount) override;

	virtual double GetNetworkDiff() const;
	virtual unsigned char * GetData();
	virtual const unsigned char * GetData() const;
	virtual uint32_t GetDataSize() const;

public:
	Target		_target;
	uint32_t	_version = 0;
	uint32_t	_nbits = 0;
	uint32_t	_time = 0;

protected:
	std::vector<unsigned char>	_data;
	uint32_t					*_increment = nullptr;
};
