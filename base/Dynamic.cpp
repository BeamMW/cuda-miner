#include "pch.hpp"
#include "Dynamic.hpp"

//	Override methods of Manageable

ULONG STDMETHODCALLTYPE
Dynamic::AddRef (
	void
)
{
	return m_nRefCount++;
}

ULONG STDMETHODCALLTYPE
Dynamic::Release (
	void
)
{
	if (0 != m_nRefCount) {
		if (0 == m_nRefCount--) {
			delete this;
			return 0;
		}
	}

	return m_nRefCount;
}

ULONG
Dynamic::getRefCount (
	void
)
const
{
	return m_nRefCount;
}
