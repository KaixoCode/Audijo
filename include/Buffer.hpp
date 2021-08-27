#pragma once
#include "pch.hpp"

namespace Audijo
{
	template<typename T>
	struct Buffer
	{
		using Type = T;
		constexpr static inline bool floating = std::is_floating_point_v<Type>;
		struct Channel
		{
			struct Iterator
			{
				using iterator_category = std::contiguous_iterator_tag;
				using difference_type = std::ptrdiff_t;
				using value_type = Type;
				using pointer = Type*;
				using reference = Type&;

				Iterator(pointer ptr) : m_ptr(ptr) {}
				Iterator(Iterator&& other) : m_ptr(other.m_ptr) {}
				Iterator(const Iterator& other) : m_ptr(other.m_ptr) {}

				reference operator*() { return *m_ptr; }
				pointer operator->() { return m_ptr; }

				Iterator& operator++() { m_ptr++; return *this; }
				Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

				friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
				friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };

			private:
				pointer m_ptr;
			};

			Channel() {}
			Channel(Type* data, int size)
				: m_Buffer(data), m_Size(size)
			{}

			Iterator begin() { return Iterator{ &m_Buffer[0] }; }
			Iterator end() { return Iterator{ &m_Buffer[m_Size] }; }

			int BufferSize() const { return m_Size; }
			Type& operator[](int index) { return m_Buffer[index]; }

		private:
			Type* m_Buffer;
			int m_Size;
		};

		struct Iterator
		{
			using iterator_category = std::contiguous_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = Channel;
			using pointer = Channel*;
			using reference = Channel&;

			Iterator(pointer ptr) : m_ptr(ptr) {}
			Iterator(Iterator&& other) : m_ptr(other.ptr) {}
			Iterator(const Iterator& other) : m_ptr(other.ptr) {}

			reference operator*() { return *m_ptr; }
			pointer operator->() { return m_ptr; }

			Iterator& operator++() { m_ptr++; return *this; }
			Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

			friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
			friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };

		private:
			pointer m_ptr;
		};

		Buffer() {}
		Buffer(Type** data, int channels, int size)
			: m_Buffer(data), m_ChannelCount(channels), m_Size(size), m_Channels(new Channel[channels])
		{
			for (int i = 0; i < m_ChannelCount; i++)
				m_Channels[i] = Channel{ m_Buffer[i], m_Size };
		}

		Buffer& operator=(Buffer&& other)
		{
			m_Buffer = other.m_Buffer;
			m_Channels = other.m_Channels;
			m_ChannelCount = other.m_ChannelCount;
			m_Size = other.m_Size;
			other.m_ChannelCount = 0;
			return *this;
		}

		~Buffer() { if (m_ChannelCount > 0) delete[] m_Channels; }

		Iterator begin() { return Iterator{ &m_Channels[0] }; }
		Iterator end() { return Iterator{ &m_Channels[m_ChannelCount] }; }

		int ChannelCount() const { return m_ChannelCount; }
		int BufferSize() const { return m_Size; }

		Channel& operator[](int index) { return m_Channels[index]; }

	private:
		Type** m_Buffer;
		Channel* m_Channels;
		int m_ChannelCount;
		int m_Size;
	};
}