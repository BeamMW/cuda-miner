#pragma	once

#include "core/Worker.hpp"

namespace core
{
	class NullWorker : public Worker
	{
	public:
		typedef Reference<NullWorker> Ref;

	public:
		bool Start() override;
		bool Stop() override;

		Work::Ref GetWork() override;
		void SetWork(Work::Ref aWork) override;
		void CancelWork(const std::string &aId) override;
		void PostSolution(core::Solution::Ref aSolution) const override;

	protected:
		bool OnCreateTransport() override;
		bool OnConnected() override;
		bool OnDisconnected() override;
	};
}
