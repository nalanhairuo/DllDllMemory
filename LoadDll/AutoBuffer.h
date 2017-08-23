#pragma once

template <typename baseType> 
class CAutoBuffer
{
public:
	baseType*	m_p;
	size_t		m_nAllocLength;

	operator baseType*()
	{
		return m_p;
	}

	template<typename T>
	operator T*()
	{
		return (T*)m_p;
	}

	baseType& operator[](int nIndex)
	{
		return m_p[nIndex];
	}

	CAutoBuffer() : m_p(NULL), m_nAllocLength(0)
	{
	}

	CAutoBuffer(__in DWORD nInitSize)
	{
		m_p = new baseType[nInitSize];
		m_nAllocLength = nInitSize;
	}

	~CAutoBuffer()
	{
		if (m_p)
		{
			delete[] m_p;
			m_p = NULL;
		}
	}

	void Init(__in DWORD nInitSize)
	{
		if (m_p == NULL)
		{
			m_p = new baseType[nInitSize];
			m_nAllocLength = nInitSize;
		}
	}

	void Alloc(__in DWORD nAllocSize)
	{
		delete[] m_p;

		m_nAllocLength = nAllocSize;
		m_p = new baseType[m_nAllocLength];
	}

	void Expand(__in DWORD nExpandSize)
	{
		delete[] m_p;

		m_nAllocLength += nExpandSize;
		m_p = new baseType[m_nAllocLength];
	}
};

class CProfileBuffer : public CAutoBuffer<TCHAR>
{
public:
	size_t GetPrivateProfileString(__in_opt LPCTSTR lpAppName, __in_opt LPCTSTR lpKeyName, __in_opt LPCTSTR lpDefault, __in_opt LPCTSTR lpFileName, __in size_t nInitSize)
	{
		Init(nInitSize);

		size_t nDataLength;
		while (nDataLength = ::GetPrivateProfileString(lpAppName, lpKeyName, lpDefault, m_p, m_nAllocLength, lpFileName), nDataLength == m_nAllocLength - 1)
		{
			Expand(nInitSize);
		}

		return nDataLength;
	}

	size_t GetPrivateProfileSection(__in LPCTSTR lpAppName, __in_opt LPCWSTR lpFileName, __in size_t nInitSize)
	{
		Init(nInitSize);

		size_t nDataLength;
		while (nDataLength = ::GetPrivateProfileSection(lpAppName, m_p, m_nAllocLength, lpFileName), nDataLength >= m_nAllocLength - 2)
		{
			Expand(nInitSize);
		}

		return nDataLength;
	}

	size_t GetPrivateProfileSectionNames(__in_opt LPCWSTR lpFileName, __in size_t nInitSize)
	{
		Init(nInitSize);

		size_t nDataLength;
		while (nDataLength = ::GetPrivateProfileSectionNames(m_p, m_nAllocLength, lpFileName), nDataLength >= m_nAllocLength - 2)
		{
			Expand(nInitSize);
		}

		return nDataLength;
	}
};
