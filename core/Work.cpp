#include "pch.hpp"
#include "Work.hpp"

using namespace core;

uint32_t Work::GetExtraNonceSize() const
{
	return _extraNonceSize;
}

void Work::SetExtraNonceSize(uint32_t aExtraNonceSize)
{
	_extraNonceSize = aExtraNonceSize;
}
