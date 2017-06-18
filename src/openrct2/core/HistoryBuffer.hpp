#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
* OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
*
* OpenRCT2 is the work of many authors, a full list can be found in contributors.md
* For more information, visit https://github.com/OpenRCT2/OpenRCT2
*
* OpenRCT2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* A full copy of the GNU General Public License can be found in licence.txt
*****************************************************************************/
#pragma endregion

#include <vector>

template<typename T>
class HistoryBuffer
{
public:
	typedef T          value_type;
	typedef T         *pointer;
	typedef const T   *const_pointer;
	typedef T         &reference;
	typedef const T   &const_reference;
	typedef size_t     size_type;
	typedef ptrdiff_t  difference_type;

private:
	std::vector<value_type> m_data;
	size_t m_head;
	size_t m_size;

public:
	HistoryBuffer()
	{
		reset(1024);
	}

	HistoryBuffer(size_t size)
	{
		reset(size);
	}

	void reset(size_t size)
	{
		m_head = 0;
		m_data.resize(size);
		m_size = 0;
	}

	reference occupy()
	{
		if (m_size == m_data.size())
		{
			reference ref = m_data[m_head++];

			if (m_head == m_data.size())
				m_head = 0;

			return ref;
		}

		return m_data[m_size++];
	}

	void add(const value_type& v)
	{
		if (m_size == m_data.size())
		{
			m_data[m_head++] = v;
			if (m_head == m_data.size())
				m_head = 0;
		}

		m_data[m_size++] = v;
	}

	size_t size() const
	{
		return m_size;
	}

	const reference operator[](size_t n) const
	{
		size_t idx = (m_head + n) % m_data.size();
		return m_data[idx];
	}

	reference operator[](size_t n)
	{
		size_t idx = (m_head + n) % m_data.size();
		return m_data[idx];
	}

};
