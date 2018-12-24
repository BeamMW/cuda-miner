
#pragma	once

template <
	class T 
>
class Reference
{
	protected: T*	m_pT;

	public:
	Reference (
		void
	)
	:	m_pT(NULL)
	{
	}

	public:
	Reference (
		const T& ref
	)
	:	m_pT(const_cast<T*>(&ref))
	{
		if (NULL != m_pT) {
			m_pT->AddRef();
		}
	}

	public:
	Reference (
		const T* p
	)
	:	m_pT(const_cast<T*>(p))
	{
		if (NULL != m_pT) {
			m_pT->AddRef();
		}
	}

	public:
	Reference (
		const Reference& ref
	)
	:	m_pT(const_cast<Reference&>(ref).m_pT)
	{
		if (NULL != m_pT) {
			m_pT->AddRef();
		}
	}

	public:
	~Reference (
		void
	)
	{
		release();
	}

	public:
	T*
	get (
		void
	)
	{
		return m_pT;
	}

	public:
	const T*
	get (
		void
	)
	const
	{
		return m_pT;
	}

	public:
	void
	release (
		void
	)
	const
	{
		if (NULL != m_pT) {
			const_cast<T*>(m_pT)->Release();
			const_cast<T*&>(m_pT) = NULL;
		}
	}

	public:
	const Reference&
	set (
		const T* p
	)
	{
		release();

		m_pT = const_cast<T*>(p);

		if (NULL != m_pT) {
			const_cast<T*>(m_pT)->AddRef();
		}

		return *this;
	}

	public:
	operator bool (
		void
	)
	const
	{
		return NULL != m_pT;
	}

	public:
	T&
	operator * (
		void
	)
	{
		return *m_pT;
	}

	public:
	const T&
	operator * (
		void
	)
	const
	{
		return *m_pT;
	}

	public:
	T*
	operator -> (
		void
	)
	{
		return m_pT;
	}

	public:
	const T*
	operator -> (
		void
	)
	const
	{
		return m_pT;
	}

	public:
	bool
	operator == (
		const Reference& ref
	)
	const
	{
		return m_pT == ref.m_pT;
	}

	public:
	bool
	operator == (
		const T* p
	)
	const
	{
		return m_pT == p;
	}

	public:
	bool
	operator != (
		const Reference& ref
	)
	const
	{
		return m_pT != ref.m_pT;
	}

	public:
	bool
	operator != (
		const T* p
	)
	const
	{
		return m_pT != p;
	}

	public:
	bool
	operator < (
		const Reference& ref
	)
	const
	{
		if (NULL == ref.m_pT)
		{
			return false;
		}
		if (NULL == m_pT)
		{
			return true;
		}
		return m_pT->operator < (*ref.m_pT);
	}

	public:
	const Reference&
	operator = (
		const Reference& ref
	)
	const
	{
		const_cast<Reference*>(this)->set(ref.m_pT);

		return *this;
	}
};

