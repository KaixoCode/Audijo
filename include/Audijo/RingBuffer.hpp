#pragma once
namespace Audijo
{
	template <typename T>
	class RingBuffer {
	private:
		T* m_Buffer;

		size_t m_Head = 0;
		size_t m_Tail = 0;
		size_t m_Count = 0;
		size_t m_MaxSize;
		T m_EmptyItem = 0;
	public:

		RingBuffer(size_t max_size)
			: m_Buffer(new T[max_size]), m_MaxSize(max_size) {};

		~RingBuffer() { delete[] m_Buffer; }

		void Enqueue(T item)
		{
			if (IsFull())
				Dequeue();

			m_Buffer[m_Tail] = item;
			m_Tail = (m_Tail + 1) % m_MaxSize;
			m_Count++;
		}

		T Dequeue()
		{
			if (IsEmpty())
				return m_EmptyItem;

			T item = m_Buffer[m_Head];
			m_Buffer[m_Head] = m_EmptyItem;
			m_Head = (m_Head + 1) % m_MaxSize;
			m_Count--;
			return item;
		}

		auto Front() { return m_Buffer[m_Head]; }
		bool IsEmpty() { return m_Count == 0; }
		bool IsFull() { return m_Count == m_MaxSize; }
		int Space() { return m_MaxSize - m_Count - 1; }
		size_t Size() { return m_Count; }
	};
}