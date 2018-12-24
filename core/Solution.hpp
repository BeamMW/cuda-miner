#pragma	once

#include "base/Dynamic.hpp"
#include "base/Reference.hpp"
#include "base/BigInteger.hpp"

namespace core
{
	class Solution : public Dynamic
	{
	public:
		typedef Reference<Solution> Ref;

	public:
		virtual const BigInteger & GetHash() = 0;
		virtual bool ProofOfWork() = 0;
		virtual const std::string & GetDeviceId() const = 0;
		virtual const std::string & GetWorkId() const = 0;
		virtual void PrintTime(std::string &aDst) const = 0;
		virtual void PrintNonce(std::string &aDst, bool aBigEndian) const = 0;
		virtual void PrintSolution(std::string &aDst) const = 0;

		virtual void OnAccepted(unsigned aDuration) = 0;
		virtual void OnRejected(unsigned aDuration, const std::string &aReason) = 0;
	};
}
