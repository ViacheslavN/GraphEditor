#pragma once

#include <memory>
#include <iterator>
#include <algorithm>
#include <utility>
#include <stdexcept>

namespace bptreedb
{
	// Double-ended vector: contiguous storage with free space at both ends, the subset of the
	// std::vector interface used by the nodes and the encoders.
	//  - erase / insert at index 0 moves the start of the data instead of the whole data;
	//  - erase / insert in the middle moves the shorter side;
	//  - push_back as std::vector.
	// So a leaf that loses or gets keys at the front (removing keys in order, inserting in reverse order)
	// pays O(1) per change instead of O(n). Iterators are raw pointers (random access), they are
	// invalidated by every change, as for std::vector.
	template<class T, class TAlloc = std::allocator<T>>
	class TDeVector
	{
	public:
		typedef T value_type;
		typedef TAlloc allocator_type;
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;
		typedef T& reference;
		typedef const T& const_reference;
		typedef T* pointer;
		typedef const T* const_pointer;
		typedef T* iterator;
		typedef const T* const_iterator;

	private:
		typedef std::allocator_traits<TAlloc> TTraits;

	public:
		TDeVector() = default;

		explicit TDeVector(const TAlloc& alloc) : m_alloc(alloc)
		{}

		// allocators are always direct-initialized: CommonLib::STLAllocator has an explicit copy constructor
		TDeVector(const TDeVector& other) : m_alloc(other.m_alloc)
		{
			reserve(other.size());
			for (const T& val : other)
				TTraits::construct(m_alloc, m_end++, val);
		}

		TDeVector(TDeVector&& other) noexcept : m_alloc(std::move(other.m_alloc)),
			m_buf(other.m_buf), m_cap(other.m_cap), m_begin(other.m_begin), m_end(other.m_end)
		{
			other.m_buf = other.m_begin = other.m_end = nullptr;
			other.m_cap = 0;
		}

		TDeVector& operator=(const TDeVector& other)
		{
			if (this != &other)
			{
				TDeVector tmp(other);
				swap(tmp);
			}
			return *this;
		}

		TDeVector& operator=(TDeVector&& other) noexcept
		{
			if (this != &other)
			{
				Free();
				m_alloc = std::move(other.m_alloc);
				m_buf = other.m_buf; m_cap = other.m_cap; m_begin = other.m_begin; m_end = other.m_end;
				other.m_buf = other.m_begin = other.m_end = nullptr;
				other.m_cap = 0;
			}
			return *this;
		}

		~TDeVector()
		{
			Free();
		}

		allocator_type get_allocator() const { return allocator_type(m_alloc); }

		iterator begin() { return m_begin; }
		iterator end() { return m_end; }
		const_iterator begin() const { return m_begin; }
		const_iterator end() const { return m_end; }

		size_type size() const { return size_type(m_end - m_begin); }
		bool empty() const { return m_begin == m_end; }
		size_type capacity() const { return m_cap; }

		T& operator[](size_type i) { return m_begin[i]; }
		const T& operator[](size_type i) const { return m_begin[i]; }
		T& front() { return *m_begin; }
		const T& front() const { return *m_begin; }
		T& back() { return *(m_end - 1); }
		const T& back() const { return *(m_end - 1); }
		T* data() { return m_begin; }
		const T* data() const { return m_begin; }

		void swap(TDeVector& other) noexcept
		{
			using std::swap;
			TAlloc tmpAlloc(m_alloc);
			m_alloc = other.m_alloc;
			other.m_alloc = tmpAlloc;
			swap(m_buf, other.m_buf);
			swap(m_cap, other.m_cap);
			swap(m_begin, other.m_begin);
			swap(m_end, other.m_end);
		}

		void clear()
		{
			Destroy(m_begin, m_end);
			m_begin = m_end = m_buf;
		}

		void reserve(size_type n)
		{
			if (n > size_type(BufEnd() - m_begin))
				Reallocate(n, size_type(0));
		}

		void resize(size_type n)
		{
			if (n < size())
			{
				Destroy(m_begin + n, m_end);
				m_end = m_begin + n;
				return;
			}
			ReserveBack(n - size());
			while (size() < n)
				TTraits::construct(m_alloc, m_end++);
		}

		void resize(size_type n, const T& val)
		{
			if (n < size())
			{
				Destroy(m_begin + n, m_end);
				m_end = m_begin + n;
				return;
			}
			T copy(val);
			ReserveBack(n - size());
			while (size() < n)
				TTraits::construct(m_alloc, m_end++, copy);
		}

		void push_back(const T& val)
		{
			if (m_end == BufEnd())
			{
				T copy(val);
				ReserveBack(1);
				TTraits::construct(m_alloc, m_end++, std::move(copy));
				return;
			}
			TTraits::construct(m_alloc, m_end++, val);
		}

		void push_back(T&& val)
		{
			if (m_end == BufEnd())
			{
				T copy(std::move(val));
				ReserveBack(1);
				TTraits::construct(m_alloc, m_end++, std::move(copy));
				return;
			}
			TTraits::construct(m_alloc, m_end++, std::move(val));
		}

		template<class... TArgs>
		void emplace_back(TArgs&&... args)
		{
			push_back(T(std::forward<TArgs>(args)...));
		}

		void pop_back()
		{
			--m_end;
			TTraits::destroy(m_alloc, m_end);
		}

		iterator insert(const_iterator pos, const T& val)
		{
			T copy(val);
			return InsertOne(size_type(pos - m_begin), std::move(copy));
		}

		iterator insert(const_iterator pos, T&& val)
		{
			T copy(std::move(val));
			return InsertOne(size_type(pos - m_begin), std::move(copy));
		}

		template<class TInputIt, class = typename std::iterator_traits<TInputIt>::iterator_category>
		iterator insert(const_iterator pos, TInputIt first, TInputIt last)
		{
			size_type idx = size_type(pos - m_begin);
			if (idx == size())
			{
				// append (the only case the nodes use): one reservation
				size_type n = size_type(std::distance(first, last));
				ReserveBack(n);
				for (; first != last; ++first)
					TTraits::construct(m_alloc, m_end++, *first);
				return m_begin + idx;
			}

			TDeVector tmp(m_alloc);
			tmp.reserve(size() + size_type(std::distance(first, last)));
			for (size_type i = 0; i < idx; ++i)
				tmp.push_back(std::move(m_begin[i]));
			for (; first != last; ++first)
				tmp.push_back(*first);
			for (size_type i = idx, sz = size(); i < sz; ++i)
				tmp.push_back(std::move(m_begin[i]));
			swap(tmp);
			return m_begin + idx;
		}

		iterator erase(const_iterator pos)
		{
			size_type idx = size_type(pos - m_begin);
			if (idx < size() - idx - 1)
			{
				// the front part is shorter: shift it right by one
				std::move_backward(m_begin, m_begin + idx, m_begin + idx + 1);
				TTraits::destroy(m_alloc, m_begin);
				++m_begin;
			}
			else
			{
				std::move(m_begin + idx + 1, m_end, m_begin + idx);
				--m_end;
				TTraits::destroy(m_alloc, m_end);
			}
			if (m_begin == m_end)
				m_begin = m_end = m_buf;   // empty: all space at the back again
			return m_begin + idx;
		}

		iterator erase(const_iterator first, const_iterator last)
		{
			size_type idx = size_type(first - m_begin);
			size_type n = size_type(last - first);
			if (n == 0)
				return m_begin + idx;

			if (idx < size() - idx - n)
			{
				std::move_backward(m_begin, m_begin + idx, m_begin + idx + n);
				Destroy(m_begin, m_begin + n);
				m_begin += n;
			}
			else
			{
				std::move(m_begin + idx + n, m_end, m_begin + idx);
				Destroy(m_end - n, m_end);
				m_end -= n;
			}
			if (m_begin == m_end)
				m_begin = m_end = m_buf;
			return m_begin + idx;
		}

	private:

		T* BufEnd() const { return m_buf + m_cap; }

		iterator InsertOne(size_type idx, T&& val)
		{
			size_type sz = size();
			bool bFront = idx < sz - idx;     // the front part is shorter

			if (bFront && m_begin == m_buf)
			{
				if (m_end != BufEnd() && sz - idx <= idx + 64)
					bFront = false;           // no room at the front, use the back if it isn't much worse
				else
					ReserveFront(1);
			}
			if (!bFront && m_end == BufEnd())
				ReserveBack(1);

			if (bFront)
			{
				if (idx == 0)
					TTraits::construct(m_alloc, m_begin - 1, std::move(val));
				else
				{
					TTraits::construct(m_alloc, m_begin - 1, std::move(m_begin[0]));
					std::move(m_begin + 1, m_begin + idx, m_begin);
					m_begin[idx - 1] = std::move(val);
				}
				--m_begin;
				return m_begin + idx;
			}

			if (idx == sz)
				TTraits::construct(m_alloc, m_end, std::move(val));
			else
			{
				TTraits::construct(m_alloc, m_end, std::move(*(m_end - 1)));
				std::move_backward(m_begin + idx, m_end - 1, m_end);
				m_begin[idx] = std::move(val);
			}
			++m_end;
			return m_begin + idx;
		}

		// room for n more elements at the back
		void ReserveBack(size_type n)
		{
			if (size_type(BufEnd() - m_end) >= n)
				return;

			size_type sz = size();
			size_type nFront = size_type(m_begin - m_buf);
			// a big free front (left by front erases): shift the data down instead of growing
			if (nFront >= n && nFront >= sz / 2 && sz + n <= m_cap)
			{
				Shift(m_buf);
				return;
			}
			Reallocate((std::max)(sz + n, m_cap * 2), size_type(0));
		}

		// room for n more elements at the front
		void ReserveFront(size_type n)
		{
			if (size_type(m_begin - m_buf) >= n)
				return;

			size_type sz = size();
			size_type nBack = size_type(BufEnd() - m_end);
			if (nBack >= n && nBack >= sz / 2 && sz + n <= m_cap)
			{
				// center the data in the existing buffer
				Shift(m_buf + (m_cap - sz) / 2);
				return;
			}
			size_type newCap = (std::max)(sz + n, m_cap * 2);
			Reallocate(newCap, (newCap - sz) / 2);
		}

		// moves the data to start at pNewBegin inside the buffer
		void Shift(T* pNewBegin)
		{
			size_type sz = size();
			if (pNewBegin == m_begin)
				return;

			if (pNewBegin < m_begin)
			{
				// moving down: construct the part below the old begin, assign the overlapping part
				T* src = m_begin;
				T* dst = pNewBegin;
				T* constructedEnd = m_begin;     // [pNewBegin, m_begin) isn't constructed
				for (size_type i = 0; i < sz; ++i, ++src, ++dst)
				{
					if (dst < constructedEnd)
						TTraits::construct(m_alloc, dst, std::move(*src));
					else
						*dst = std::move(*src);
				}
				T* newEnd = pNewBegin + sz;
				Destroy((std::max)(newEnd, m_begin), m_end);
				m_begin = pNewBegin;
				m_end = newEnd;
			}
			else
			{
				T* newEnd = pNewBegin + sz;
				T* src = m_end;
				T* dst = newEnd;
				for (size_type i = 0; i < sz; ++i)
				{
					--src; --dst;
					if (dst >= m_end)
						TTraits::construct(m_alloc, dst, std::move(*src));
					else
						*dst = std::move(*src);
				}
				Destroy(m_begin, (std::min)(pNewBegin, m_end));
				m_begin = pNewBegin;
				m_end = newEnd;
			}
		}

		void Reallocate(size_type newCap, size_type nFront)
		{
			if (newCap < 16)
				newCap = 16;
			size_type sz = size();
			if (nFront + sz > newCap)
				nFront = newCap - sz;

			T* pNew = TTraits::allocate(m_alloc, newCap);
			T* pBegin = pNew + nFront;
			T* pEnd = pBegin;
			try
			{
				for (T* p = m_begin; p != m_end; ++p, ++pEnd)
					TTraits::construct(m_alloc, pEnd, std::move_if_noexcept(*p));
			}
			catch (...)
			{
				Destroy(pBegin, pEnd);
				TTraits::deallocate(m_alloc, pNew, newCap);
				throw;
			}

			Free();
			m_buf = pNew;
			m_cap = newCap;
			m_begin = pBegin;
			m_end = pEnd;
		}

		void Destroy(T* first, T* last)
		{
			for (; first < last; ++first)
				TTraits::destroy(m_alloc, first);
		}

		void Free()
		{
			if (!m_buf)
				return;
			Destroy(m_begin, m_end);
			TTraits::deallocate(m_alloc, m_buf, m_cap);
			m_buf = m_begin = m_end = nullptr;
			m_cap = 0;
		}

	private:
		TAlloc m_alloc;
		T* m_buf{ nullptr };
		size_type m_cap{ 0 };
		T* m_begin{ nullptr };
		T* m_end{ nullptr };
	};

	template<class T, class TAlloc>
	void swap(TDeVector<T, TAlloc>& a, TDeVector<T, TAlloc>& b) noexcept
	{
		a.swap(b);
	}
}
