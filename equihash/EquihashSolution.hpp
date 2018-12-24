#pragma	once

#include "core/Solution.hpp"
#include "core/Work.hpp"
#include "core/Statistics.hpp"
#include "EquihashWork.hpp"

class EquihashSolution : public core::Solution
{
public:
	typedef Reference<EquihashSolution> Ref;

public:
	EquihashSolution(const std::string &aDeviceId, const EquihashWork &aWork, core::Statistics *aStatistics);
	EquihashSolution(
		const std::string &aDeviceId,
		uint32_t aVersion,
		const std::string &aPrevHash,
		const std::string &aMerkleRoot,
		uint32_t aTime,
		const std::string &aBits,
		const std::string &aNonce,
		const std::string &aSolution
		);

	const BigInteger & GetHash() override;
	bool ProofOfWork() override;
	const std::string & GetDeviceId() const override;
	const std::string & GetWorkId() const override;
	void PrintTime(std::string &aDst) const override;
	void PrintNonce(std::string &aDst, bool aBigEndian) const override;
	void PrintSolution(std::string &aDst) const override;

	void OnAccepted(unsigned aDuration) override;
	void OnRejected(unsigned aDuration, const std::string &aReason) override;

	bool Add(const std::vector<uint32_t> &aIndexVector, size_t aCbitlen);
	bool CheckSolution(const std::vector<uint32_t> &aIndexVector, size_t aCbitlen);

protected:
	std::string		_deviceId;
	std::string		_workId;
	core::Statistics	*_statistics = nullptr;
	BigInteger		_hash;
	uint32_t		_extraNonceSize = 0;
	unsigned char	_header[140 + 3 + 1344];
};
