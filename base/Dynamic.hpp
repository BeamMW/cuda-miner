
#pragma	once

#include <atomic>
#include "Manageable.hpp"

class Dynamic :
	public	Manageable
{
private:
	std::atomic_long	m_nRefCount;

public:
	__inline Dynamic() : m_nRefCount(0) {}

	__inline Dynamic(const Dynamic &) : m_nRefCount(0) {}

	//	Override methods of Manageable

	unsigned long AddRef() override;
	unsigned long Release() override;
	unsigned long getRefCount() const override;
};

