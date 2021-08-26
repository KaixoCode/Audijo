#include "ApiBase.hpp"

namespace Audijo
{
	template<typename In, typename Out>
	void ConvertBufferTyped(char* outBuffer, char* inBuffer, size_t bufferSize)
	{
		Out* out = (Out*)outBuffer;
		In* in = (In*)inBuffer;
		for (size_t i = 0; i < bufferSize; i++)
		{
			if constexpr (std::is_integral_v<In> && std::is_integral_v<Out>)
			{
				out[i] = (Out)in[i];
				if (sizeof(Out) - sizeof(In) > 0)
					out[i] <<= sizeof(Out) - sizeof(In);
			}
			else if constexpr (std::is_floating_point_v<In> && std::is_floating_point_v<Out>)
				out[i] = (Out)in[i];

			else if constexpr (std::is_floating_point_v<Out>)
				out[i] = (Out)in[i] / (Out)std::numeric_limits<In>::max();

			else
				out[i] = (Out)(in[i] * std::numeric_limits<Out>::max());
		}
	}

	void ApiBase::AllocateBuffers()
	{
		int _nInChannels = m_Settings.input.channels;
		int _nOutChannels = m_Settings.output.channels;
		int _nChannels = _nInChannels + _nOutChannels;
		int _bufferSize = m_Settings.bufferSize;
		double _sampleRate = m_Settings.sampleRate;
		auto _inFormat = m_Settings.m_DeviceInFormat;
		auto _inSwap = m_Settings.m_InByteSwap;
		auto _outFormat = m_Settings.m_DeviceOutFormat;
		auto _outSwap = m_Settings.m_OutByteSwap;
		auto _format = m_Settings.m_Format;

		m_InputBuffers = new char* [_nInChannels];
		m_OutputBuffers = new char* [_nOutChannels];

		for (int i = 0; i < _nInChannels; i++)
			m_InputBuffers[i] = new char[_bufferSize * FormatBytes(_format)];

		for (int i = 0; i < _nOutChannels; i++)
			m_OutputBuffers[i] = new char[_bufferSize * FormatBytes(_format)];
	}

	void ApiBase::FreeBuffers()
	{
		if (m_InputBuffers != nullptr)
		{
			int _nInChannels = m_Settings.input.channels;
			for (int i = 0; i < _nInChannels; i++)
				delete[] m_InputBuffers[i];
			delete[] m_InputBuffers;
		}

		if (m_OutputBuffers != nullptr)
		{
			int _nOutChannels = m_Settings.output.channels;
			for (int i = 0; i < _nOutChannels; i++)
				delete[] m_OutputBuffers[i];
			delete[] m_OutputBuffers;
		}
	}

	void ApiBase::ConvertBuffer(char* outBuffer, char* inBuffer, size_t bufferSize, SampleFormat outFormat, SampleFormat inFormat)
	{
		switch (inFormat)
		{
		case Int8:
			switch (outFormat)
			{
			case Int16:   ConvertBufferTyped<int8_t, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<int8_t, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<int8_t, float>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<int8_t, double>(outBuffer, inBuffer, bufferSize); break;
			default: memcpy(outBuffer, inBuffer, bufferSize);
			} break;
		case Int16:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<int16_t, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<int16_t, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<int16_t, float>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<int16_t, double>(outBuffer, inBuffer, bufferSize); break;
			default: memcpy(outBuffer, inBuffer, bufferSize * 2);
			} break;
		case Int32:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<int32_t, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int16:   ConvertBufferTyped<int32_t, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<int32_t, float>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<int32_t, double>(outBuffer, inBuffer, bufferSize); break;
			default: memcpy(outBuffer, inBuffer, bufferSize * 4);
			} break;
		case Float32:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<float, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int16:   ConvertBufferTyped<float, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<float, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float64: ConvertBufferTyped<float, double>(outBuffer, inBuffer, bufferSize); break;
			default: memcpy(outBuffer, inBuffer, bufferSize * 4);
			} break;
		case Float64:
			switch (outFormat)
			{
			case Int8:    ConvertBufferTyped<double, int8_t>(outBuffer, inBuffer, bufferSize); break;
			case Int16:   ConvertBufferTyped<double, int16_t>(outBuffer, inBuffer, bufferSize); break;
			case Int32:   ConvertBufferTyped<double, int32_t>(outBuffer, inBuffer, bufferSize); break;
			case Float32: ConvertBufferTyped<double, float>(outBuffer, inBuffer, bufferSize); break;
			default: memcpy(outBuffer, inBuffer, bufferSize * 8);
			} break;
		}
	}

	void ApiBase::ByteSwapBuffer(char* buffer, unsigned int bufferSize, SampleFormat format)
	{
		char val;
		char* ptr;

		ptr = buffer;
		if (format == Int16)
		{
			for (unsigned int i = 0; i < bufferSize; i++)
			{
				// Swap 1st and 2nd bytes.
				val = *(ptr);
				*(ptr) = *(ptr + 1);
				*(ptr + 1) = val;

				// Increment 2 bytes.
				ptr += 2;
			}
		}
		else if (format == Int32 || format == Float32)
		{
			for (unsigned int i = 0; i < bufferSize; i++)
			{
				// Swap 1st and 4th bytes.
				val = *(ptr);
				*(ptr) = *(ptr + 3);
				*(ptr + 3) = val;

				// Swap 2nd and 3rd bytes.
				ptr += 1;
				val = *(ptr);
				*(ptr) = *(ptr + 1);
				*(ptr + 1) = val;

				// Increment 3 more bytes.
				ptr += 3;
			}
		}
		else if (format == Float64)
		{
			for (unsigned int i = 0; i < bufferSize; i++)
			{
				// Swap 1st and 8th bytes
				val = *(ptr);
				*(ptr) = *(ptr + 7);
				*(ptr + 7) = val;

				// Swap 2nd and 7th bytes
				ptr += 1;
				val = *(ptr);
				*(ptr) = *(ptr + 5);
				*(ptr + 5) = val;

				// Swap 3rd and 6th bytes
				ptr += 1;
				val = *(ptr);
				*(ptr) = *(ptr + 3);
				*(ptr + 3) = val;

				// Swap 4th and 5th bytes
				ptr += 1;
				val = *(ptr);
				*(ptr) = *(ptr + 1);
				*(ptr + 1) = val;

				// Increment 5 more bytes.
				ptr += 5;
			}
		}
	}

	unsigned int ApiBase::FormatBytes(SampleFormat format)
	{
		return format & 0xF;
	}
}