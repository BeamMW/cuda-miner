#pragma	once

#include "core/Worker.hpp"
#include "core/Miner.hpp"

namespace core
{
	class Farm
	{
	public:
		Farm();
		virtual ~Farm();

		virtual bool Init(int argc, char **argv);
		virtual void Run();
		virtual bool Done();

		virtual void Cancel() = 0;

		void SetWorker(Worker::Ref aWorker);

	protected:
		Worker::Ref	_worker;
		Miner::List	_miners;
	};
}
