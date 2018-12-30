#include "pch.hpp"
#include "Dynamic.hpp"

//	Override methods of Manageable

unsigned long Dynamic::AddRef()
{
	return m_nRefCount++;
}

unsigned long Dynamic::Release()
{
	if (0 != m_nRefCount) {
		if (0 == m_nRefCount--) {
			delete this;
			return 0;
		}
	}

	return m_nRefCount;
}

unsigned long Dynamic::getRefCount() const
{
	return m_nRefCount;
}
