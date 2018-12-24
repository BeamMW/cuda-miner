
#pragma	once

class Manageable
{
protected:
	virtual	~Manageable();

public:
	virtual	unsigned long AddRef() = 0;
	virtual unsigned long Release() = 0;
	virtual unsigned long getRefCount() const = 0;
};

