#pragma	once

#include "base/Dynamic.hpp"
#include "base/Reference.hpp"

namespace core
{
	class Device : public Dynamic
	{
	public:
		typedef Reference<Device> Ref;
	};
}
