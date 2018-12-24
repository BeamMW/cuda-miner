#pragma	once

#include "base/Dynamic.hpp"
#include "base/Reference.hpp"
#include "base/BigInteger.hpp"

namespace core
{
	class Work : public Dynamic
	{
	public:
		typedef Reference<Work> Ref;

	public:
		virtual unsigned char * GetNonce() = 0;
		virtual uint32_t GetNonceSize() const = 0;
		virtual uint32_t GetExtraNonceSize() const;
		virtual void SetExtraNonceSize(uint32_t aExtraNonceSize);

		virtual core::Work::Ref CreateForThread(uint32_t aThreadId) const = 0;
		virtual void Increment(unsigned aCount) = 0;

	public:
		std::string					_id;
		uint32_t					_extraNonceSize = 0;
		bool						_clean = false;
	};
}
