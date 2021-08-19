#pragma once
#include "pch.hpp"
#include "Callback.hpp"

namespace Audijo 
{
	enum SampleFormat 
	{
		None, Int8, Int16, Int32, Float32, Float64
	};

	struct DeviceInfo 
	{
		int id;
		std::string name;
		int inputChannels;
		int outputChannels;
		std::vector<double> sampleRates;
	};

	struct Parameters 
	{
		int deviceId = -1;
		int channels = -1;
	};

	struct StreamSettings
	{
		Parameters input{};  // Parameters for the input device
		Parameters output{}; // Parameters for the output device
		int bufferSize = 512;
		double sampleRate = -1;

	private:
		SampleFormat m_Format = None;
		SampleFormat m_DeviceInFormat = None;
		SampleFormat m_DeviceOutFormat = None;
		bool m_InByteSwap = false;
		bool m_OutByteSwap = false;

		friend class AsioApi;
		friend class ApiBase;
	};

	enum Error
	{
		NoError = 0,
		
		NoMemory,   // Failed to allocate memory
		NotPresent, // Device is not present

		InvalidSampleRate, // Device does not support sample rate
		InvalidBufferSize, // Device does not support buffer size

		NoCallback,     // The callback was not set

		NotOpen,        // Stream is not open
		NotRunning,     // Stream is not running
		AlreadyOpen,    // Stream is already open
		AlreadyRunning, // Stream is already running
		HardwareFail,   // Hardware fail

		InvalidDuplex,  // Combination of devices in duplex channel is invalid
	};

	class ApiBase
	{
	public:

		virtual ~ApiBase() 
		{
			// Free the callback buffers
			FreeBuffers();
		}

		/**
		 * Search for all available devices.
		 * @return all available devices given the chosen api.
		 */
		virtual const std::vector<DeviceInfo>& Devices() = 0;

		/*
		 * Open the stream.
		 * @param stream settings (optional)
		 * @return (NoError, HardwareFail, NotPresent, InvalidSampleRate, NoMemory)
		 */
		virtual Error OpenStream(const StreamSettings& settings = StreamSettings{}) = 0;

		/**
		 * Starts the flow of audio through the opened stream. Does nothing
		 * if the stream has not been opened yet.
		 * @return (NoError, NotOpen, AlreadyRunning, HardwareFail)
		 */
		virtual Error StartStream() = 0;

		/**
		 * Stop the flow of audio through the stream. Does nothing if the
		 * stream hasn't been started or opened yet.
		 */
		virtual Error StopStream() = 0;

		/**
		 * Close the stream. Also stops the stream if it hasn't been stopped yet.
		 * Does nothing if the stream hasn't been opened yet.
		 */
		virtual Error CloseStream() = 0;

		/**
		 * Set the callback.
		 * @param callback
		 */
		void SetCallback(std::unique_ptr<CallbackWrapperBase>&& callback) { m_Callback = std::move(callback); }

		/**
		 * Set the userdata
		 * @param data userdata
		 */
		template<typename T>
		void UserData(T& data) { m_UserData = &data; };

	protected:
		static inline double m_SampleRates[]{ 48000, 44100, 88200, 96000, 176400, 192000, 352800, 384000, 8000, 11025, 16000, 22050 };
		
		std::vector<DeviceInfo> m_Devices;
		std::unique_ptr<CallbackWrapperBase> m_Callback;
		void* m_UserData = nullptr;
		StreamSettings m_Settings{};

		/**
		 * Allocates memory for the input and output buffers used when
		 * calling the user callback.
		 */
		void AllocateBuffers();

		/**
		 *  Frees the memory allocated by AllocateBuffers.
		 */
		void FreeBuffers();

		/**
		 * Converts a buffer from one format to another.
		 * @param outBuffer the output buffer
		 * @param inBuffer the input buffer
		 * @param bufferSize the size of the buffer
		 * @param outFormat the format of the output buffer
		 * @param inFormat the format of the input buffer
		 */
		void ConvertBuffer(char* outBuffer, char* inBuffer, size_t bufferSize, SampleFormat outFormat, SampleFormat inFormat);

		/**
		 * Swaps the bytes in a buffer
		 * @param buffer the buffer
		 * @param bufferSize the size of the buffer
		 * @param format format of the samples in the buffer
		 */
		void ByteSwapBuffer(char* buffer, unsigned int bufferSize, SampleFormat format);

		/**
		 * Returns the amount of bytes in a sample format
		 * @param format the sample format
		 * @return amount of bytes in format
		 */
		unsigned int FormatBytes(SampleFormat format);

		char** m_InputBuffers = nullptr;
		char** m_OutputBuffers = nullptr;
	};

}