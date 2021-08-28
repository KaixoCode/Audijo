#pragma once
#include "pch.hpp"

namespace Audijo
{
	template<typename T>
	struct Buffer
	{
		using Type = T;
		constexpr static inline bool floating = std::is_floating_point_v<Type>;
		struct Frame
		{
			struct Iterator
			{
				using iterator_category = std::contiguous_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using value_type = Type;
				using pointer = Type*;
				using reference = Type&;

				Iterator(Frame& frame, int index) : m_Frame(frame), m_Index(index) {}
				Iterator(Iterator&& other) : m_Frame(other.m_Frame), m_Index(other.m_Index) {}
				Iterator(const Iterator& other) : m_Frame(other.m_Frame), m_Index(other.m_Index) {}

				reference operator*() { return m_Frame[m_Index]; }
				pointer operator->() { return &m_Frame[m_Index]; }

				Iterator& operator++() { m_Index++; return *this; }
				Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

				friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; };
				friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_Index != b.m_Index; };

			private:
				Frame& m_Frame;
				int m_Index;
			};

			Frame(Buffer& data, int index)
				: m_Buffer(data), m_Index(index)
			{}

			Iterator begin() { return Iterator{ *this, 0 }; }
			Iterator end() { return Iterator{ *this, m_Buffer.Channels() }; }

			int Channels() const { return m_Buffer.Channels(); }
			int Index() const { return m_Index; }
			Type& operator[](int index) { return m_Buffer.m_Buffer[index][Index()]; }

		private:
			Buffer& m_Buffer;
			int m_Index;

			friend class Buffer<Type>::Iterator;
		};

		struct Iterator
		{
			using iterator_category = std::contiguous_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = Frame;
			using pointer = Frame*;
			using reference = Frame&;

			Iterator(Frame ptr) : m_ptr(ptr) {}
			Iterator(Iterator&& other) : m_ptr(other.m_ptr) {}
			Iterator(const Iterator& other) : m_ptr(other.m_ptr) {}

			reference operator*() { return m_ptr; }
			pointer operator->() { return &m_ptr; }

			Iterator& operator++() { m_ptr.m_Index++; return *this; }
			Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

			friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr.Index() == b.m_ptr.Index(); };
			friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr.Index() != b.m_ptr.Index(); };

		private:
			Frame m_ptr;
		};

		Buffer() {}
		Buffer(Type** data, int channels, int size)
			: m_Buffer(data), m_ChannelCount(channels), m_Size(size)
		{}

		Iterator begin() { return Iterator{ Frame{ *this, 0 } }; }
		Iterator end() { return Iterator{ Frame{ *this, Frames() } }; }

		int Channels() const { return m_ChannelCount; }
		int Frames() const { return m_Size; }

		Frame operator[](int index) { return Frame{ *this, index }; }

	private:
		Type** m_Buffer;
		int m_ChannelCount;
		int m_Size;

		friend class Buffer<Type>::Frame;
		template<typename T1, typename T2> friend class Parallel;
	};

	template<typename T1, typename T2>
	struct Parallel
	{
		using InType = T1;
		using OutType = T2;

		struct Frame
		{
			struct Iterator
			{
				using iterator_category = std::contiguous_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using reference = std::pair<InType&, OutType&>;

				Iterator(Frame& frame, int index) : m_Frame(frame), m_Index(index) {}
				Iterator(Iterator&& other) : m_Frame(other.m_Frame), m_Index(other.m_Index) {}
				Iterator(const Iterator& other) : m_Frame(other.m_Frame), m_Index(other.m_Index) {}

				reference operator*() { return m_Frame[m_Index]; }

				Iterator& operator++() { m_Index++; return *this; }
				Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

				friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; };
				friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_Index != b.m_Index; };

			private:
				Frame& m_Frame;
				int m_Index;
			};

			Frame(Parallel& data, int index)
				: m_Parallel(data), m_Index(index)
			{}

			Iterator begin() { return Iterator{ *this, 0 }; }
			Iterator end() { return Iterator{ *this, Channels() }; }

			int Channels() const { return m_Parallel.Channels(); }
			int Index() const { return m_Index; }

			std::pair<InType&, OutType&> operator[](int channel) { return { m_Parallel.m_InBuffer.m_Buffer[channel][Index()], m_Parallel.m_OutBuffer.m_Buffer[channel][Index()] }; }

		private:
			Parallel& m_Parallel;
			int m_Index;

			friend class Parallel<InType, OutType>::Iterator;
		};

		struct Iterator
		{
			using iterator_category = std::contiguous_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = Frame;
			using pointer = Frame*;
			using reference = Frame&;

			Iterator(Frame ptr) : m_ptr(ptr) {}
			Iterator(Iterator&& other) : m_ptr(other.m_ptr) {}
			Iterator(const Iterator& other) : m_ptr(other.m_ptr) {}

			reference operator*() { return m_ptr; }
			pointer operator->() { return &m_ptr; }

			Iterator& operator++() { m_ptr.m_Index++; return *this; }
			Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

			friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr.Index() == b.m_ptr.Index(); };
			friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr.Index() != b.m_ptr.Index(); };

		private:
			Frame m_ptr;
		};

		Parallel(Buffer<T1>& a, Buffer<T2>& b)
			: m_InBuffer(a), m_OutBuffer(b)
		{}

		Iterator begin() { return Iterator{ Frame{ *this, 0 } }; }
		Iterator end() { return Iterator{ Frame{ *this, Frames() } }; }

		int Channels() const { return std::min(m_InBuffer.Channels(), m_OutBuffer.Channels()); }
		int Frames() const { return std::min(m_InBuffer.Frames(), m_OutBuffer.Frames()); }

		Frame operator[](int index) { return Frame{ *this, index }; }

	private:
		Buffer<T1>& m_InBuffer;
		Buffer<T2>& m_OutBuffer;
	};
}