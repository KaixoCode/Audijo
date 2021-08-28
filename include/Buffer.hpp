#pragma once
#include "pch.hpp"

namespace Audijo
{
	/**
	 * Audio buffer.
	 * @tparam T sample type
	 */
	template<typename T>
	class Buffer
	{
	public:
		using Type = T;
		constexpr static inline bool floating = std::is_floating_point_v<Type>;

		/**
		 * Holds a single frame of the buffer.
		 */
		struct Frame
		{
			/**
			 * Frame iterator.
			 */
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
				Iterator operator++(int) const { Iterator tmp = *this; ++(*this); return tmp; }
				Iterator& operator--() { m_Index--; return *this; }
				Iterator operator--(int) const { Iterator tmp = *this; --(*this); return tmp; }
				Iterator& operator+=(int v) { m_Index += v; return *this; }
				Iterator& operator-=(int v) { m_Index -= v; return *this; }
				Iterator operator+(int v) const { Iterator tmp = *this; tmp.m_Index += v; return tmp; }
				Iterator operator-(int v) const { Iterator tmp = *this; tmp.m_Index -= v; return tmp; }

				friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; };
				friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_Index != b.m_Index; };

			private:
				Frame& m_Frame;
				int m_Index;
			};

			/**
			 * Constructor.
			 * @param data buffer object this frame belongs to
			 * @param index index of this frame in the buffer
			 */
			Frame(Buffer& data, int index)
				: m_Buffer(data), m_Index(index)
			{}

			/**
			 * Amount of channels in this frame.
			 * @return channel count
			 */
			int Channels() const { return m_Buffer.Channels(); }

			/**
			 * Index of this frame in the buffer.
			 * @return index of this frame
			 */
			int Index() const { return m_Index; }

			/**
			 * Get the sample in this frame at the given channel index.
			 * @param index channel index
			 * @return sample
			 */
			Type& operator[](int index) { return m_Buffer.m_Buffer[index][Index()]; }

			Iterator begin() { return Iterator{ *this, 0 }; }
			Iterator end() { return Iterator{ *this, m_Buffer.Channels() }; }

		private:
			Buffer& m_Buffer;
			int m_Index;

			friend class Buffer<Type>::Iterator;
		};

		/**
		 * Buffer iterator.
		 */
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
			Iterator operator++(int) const { Iterator tmp = *this; ++(*this); return tmp; }
			Iterator& operator--() { m_ptr.m_Index--; return *this; }
			Iterator operator--(int) const { Iterator tmp = *this; --(*this); return tmp; }
			Iterator& operator+=(int v) { m_ptr.m_Index += v; return *this; }
			Iterator& operator-=(int v) { m_ptr.m_Index -= v; return *this; }
			Iterator operator+(int v) const { Iterator tmp = *this; tmp.m_ptr.m_Index += v; return tmp; }
			Iterator operator-(int v) const { Iterator tmp = *this; tmp.m_ptr.m_Index -= v; return tmp; }

			friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr.Index() == b.m_ptr.Index(); };
			friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr.Index() != b.m_ptr.Index(); };

		private:
			Frame m_ptr;
		};

		/**
		 * Constructor.
		 */
		Buffer() 
			: m_Buffer(nullptr), m_ChannelCount(0), m_Size(0)
		{}

		/**
		 * Constructor.
		 * @param data buffer
		 * @param channels amount of channels in the buffer
		 * @param size amount of frames in the buffer
		 */
		Buffer(Type** data, int channels, int size)
			: m_Buffer(data), m_ChannelCount(channels), m_Size(size)
		{}

		/**
		 * Amount of channels in this buffer.
		 * @return channel count
		 */
		int Channels() const { return m_ChannelCount; }

		/**
		 * Amount of frames in this buffer.
		 * @return frame count
		 */
		int Frames() const { return m_Size; }

		/**
		 * Get frame at index.
		 * @param index index
		 * @return frame at index
		 */
		Frame operator[](int index) const { return Frame{ *this, index }; }

		Iterator begin() { return Iterator{ Frame{ *this, 0 } }; }
		Iterator end() { return Iterator{ Frame{ *this, Frames() } }; }

	private:
		Type** m_Buffer;
		int m_ChannelCount;
		int m_Size;

		friend class Buffer<Type>::Frame;
		template<typename T1, typename T2> friend class Parallel;
	};

	/**
	 * Parallel class for parallel iteration of 2 buffers.
	 * @tparam T1 sample type of input buffer
	 * @tparam T2 sample type of output buffer
	 */
	template<typename T1, typename T2>
	class Parallel
	{
	public:
		using InType = T1;
		using OutType = T2;

		struct Frame
		{
			/**
			 * Frame iterator.
			 */
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
				Iterator operator++(int) const { Iterator tmp = *this; ++(*this); return tmp; }
				Iterator& operator--() { m_Index--; return *this; }
				Iterator operator--(int) const { Iterator tmp = *this; --(*this); return tmp; }
				Iterator& operator+=(int v) { m_Index += v; return *this; }
				Iterator& operator-=(int v) { m_Index -= v; return *this; }
				Iterator operator+(int v) const { Iterator tmp = *this; tmp.m_Index += v; return tmp; }
				Iterator operator-(int v) const { Iterator tmp = *this; tmp.m_Index -= v; return tmp; }

				friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; };
				friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_Index != b.m_Index; };

			private:
				Frame& m_Frame;
				int m_Index;
			};

			Frame(Parallel& data, int index)
				: m_Parallel(data), m_Index(index)
			{}

			/**
			 * Amount of channels in this frame.
			 * @return channel count
			 */
			int Channels() const { return m_Parallel.Channels(); }

			/**
			 * Index of this frame in the buffer.
			 * @return index of this frame
			 */
			int Index() const { return m_Index; }

			/**
			 * Get the samples in this parallel frame at the given channel index.
			 * @param index channel index
			 * @return pair of samples
			 */
			std::pair<InType&, OutType&> operator[](int index) { return { m_Parallel.m_InBuffer.m_Buffer[index][Index()], m_Parallel.m_OutBuffer.m_Buffer[index][Index()] }; }

			Iterator begin() { return Iterator{ *this, 0 }; }
			Iterator end() { return Iterator{ *this, Channels() }; }

		private:
			Parallel& m_Parallel;
			int m_Index;

			friend class Parallel<InType, OutType>::Iterator;
		};

		/**
		 * Parallel iterator.
		 */
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
			Iterator operator++(int) const { Iterator tmp = *this; ++(*this); return tmp; }
			Iterator& operator--() { m_ptr.m_Index--; return *this; }
			Iterator operator--(int) const { Iterator tmp = *this; --(*this); return tmp; }
			Iterator& operator+=(int v) { m_ptr.m_Index += v; return *this; }
			Iterator& operator-=(int v) { m_ptr.m_Index -= v; return *this; }
			Iterator operator+(int v) const { Iterator tmp = *this; tmp.m_ptr.m_Index += v; return tmp; }
			Iterator operator-(int v) const { Iterator tmp = *this; tmp.m_ptr.m_Index -= v; return tmp; }

			friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr.Index() == b.m_ptr.Index(); };
			friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr.Index() != b.m_ptr.Index(); };

		private:
			Frame m_ptr;
		};

		/**
		 * Constructor.
		 * @param input input buffer
		 * @param output output buffer
		 */
		Parallel(Buffer<T1>& input, Buffer<T2>& output)
			: m_InBuffer(input), m_OutBuffer(output)
		{}

		Iterator begin() { return Iterator{ Frame{ *this, 0 } }; }
		Iterator end() { return Iterator{ Frame{ *this, Frames() } }; }

		/**
		 * Amount of channels in this parallel buffer. Equals the smallest number of
		 * channels out of the 2 buffers.
		 * @return channel count
		 */
		int Channels() const { return std::min(m_InBuffer.Channels(), m_OutBuffer.Channels()); }

		/**
		 * Amount of frames in this parallel buffer. Equals the smallest number of 
		 * frames out of the 2 buffers.
		 * @return frame count
		 */
		int Frames() const { return std::min(m_InBuffer.Frames(), m_OutBuffer.Frames()); }

		/**
		 * Get a specific frame from this parallel buffer.
		 * @param index frame index
		 * @return frame
		 */
		Frame operator[](int index) const { return Frame{ *this, index }; }

	private:
		Buffer<T1>& m_InBuffer;
		Buffer<T2>& m_OutBuffer;
	};
}