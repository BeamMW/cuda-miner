#pragma	once

#include "core/Solution.hpp"
#include "core/Work.hpp"
#include "core/Statistics.hpp"
#include "BeamWork.hpp"

class BeamSolution : public core::Solution
{
public:
	typedef Reference<BeamSolution> Ref;

public:
	BeamSolution(const std::string &aDeviceId, const BeamWork &aWork, core::Statistics *aStatistics);

	const BigInteger & GetHash() override;
	bool ProofOfWork() override;
	const std::string & GetDeviceId() const override;
	const std::string & GetWorkId() const override;
	void PrintTime(std::string &aDst) const override;
	void PrintNonce(std::string &aDst, bool aBigEndian) const override;
	void PrintSolution(std::string &aDst) const override;

	void OnAccepted(unsigned aDuration) override;
	void OnRejected(unsigned aDuration, const std::string &aReason) override;

	void Add(const std::vector<uint32_t> &aIndexVector, size_t aCbitlen);
	bool CheckSolution(const std::vector<uint32_t> &aIndexVector, size_t aCbitlen);

protected:
	std::string					_deviceId;
	std::string					_workId;
	uint64_t					_nonce = 0;
	std::vector<unsigned char>	_solution;
	core::Statistics			*_statistics = nullptr;
	BigInteger					_hash;
};
