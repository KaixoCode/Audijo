#pragma once
#include "pch.hpp"
#include "Callback.hpp"

namespace Audijo 
{
	enum SampleFormat 
	{
		None,  // Float  Bytes
		Int8    = 0x00 | 0x01,
		Int16   = 0x00 | 0x02,
		Int32   = 0x00 | 0x04,
		Float32 = 0x10 | 0x04,
		Float64 = 0x10 | 0x08
	};

	struct DeviceInfo 
	{
		/**
		 * Device id
		 */
		int id;

		/**
		 * Device name
		 */
		std::string name;

		/**
		 * Amount of input channels
		 */
		int inputChannels;

		/**
		 * Amount of output channels
		 */
		int outputChannels;

		/**
		 * Supported sample rates
		 */
		std::vector<double> sampleRates;
	
		/**
		 * Default device
		 */
		bool defaultDevice;

		/**
		 * Sample format
		 */
		int sampleFormat;
	};

	enum Device { DefaultDevice = -2, NoDevice = -1 };

	struct Parameters 
	{
		/**
		 * Device id
		 */
		int deviceId = DefaultDevice;

	    /**
		 * Amount of channels
		 */
		int channels = DefaultDevice;
	};

	struct StreamSettings
	{
		/** 
		 * Parameters for the input device 
		 */
		Parameters input{};

		/** 
		 * Parameters for the output device 
		 */
		Parameters output{};

		/** 
		 * Buffer size 
		 */
		int bufferSize = 512;

		/** 
		 * Sample rate 
		 */
		double sampleRate = -1;

	private:
		SampleFormat m_Format = None;
		SampleFormat m_DeviceInFormat = None;
		SampleFormat m_DeviceOutFormat = None;
		bool m_InByteSwap = false;
		bool m_OutByteSwap = false;

		friend class AsioApi;
		friend class WasapiApi;
		friend class ApiBase;
	};

	enum Error
	{
		NoError = 0, // No error
		
		NoMemory,   // Failed to allocate memory
		NotPresent, // Device is not present

		InvalidSampleRate, // Device does not support sample rate
		InvalidBufferSize, // Device does not support buffer size
		
		UnsupportedSampleFormat, // Audijo does not support sample format of device

		NoCallback,     // The callback was not set

		NotOpen,        // Stream is not open
		NotRunning,     // Stream is not running
		AlreadyOpen,    // Stream is already open
		AlreadyRunning, // Stream is already running
		Fail,           // General fail, either hardware or other. (Api specific fail)

		InvalidDuplex,  // Combination of devices in duplex channel is invalid
	};

	class ApiBase
	{
	public:
		virtual ~ApiBase() { FreeBuffers(); }
		virtual const std::vector<DeviceInfo>& Devices() = 0;

		virtual Error OpenStream(const StreamSettings& settings = StreamSettings{}) = 0;
		virtual Error StartStream() = 0;
		virtual Error StopStream() = 0;
		virtual Error CloseStream() = 0;

		void SetCallback(std::unique_ptr<CallbackWrapperBase>&& callback) { m_Callback = std::move(callback); }

		template<typename T>
		void UserData(T& data) { m_UserData = &data; };

	protected:
		static inline double m_SampleRates[]{ 48000, 44100, 88200, 96000, 176400, 192000, 352800, 384000, 8000, 11025, 16000, 22050 };
		
		std::vector<DeviceInfo> m_Devices;
		std::unique_ptr<CallbackWrapperBase> m_Callback;
		void* m_UserData = nullptr;
		StreamSettings m_Settings{};

		void AllocateBuffers();
		void FreeBuffers();

		void ConvertBuffer(char* outBuffer, char* inBuffer, size_t bufferSize, SampleFormat outFormat, SampleFormat inFormat);
		void ByteSwapBuffer(char* buffer, unsigned int bufferSize, SampleFormat format);
		unsigned int FormatBytes(SampleFormat format);

		char** m_InputBuffers = nullptr;
		char** m_OutputBuffers = nullptr;
	};
}