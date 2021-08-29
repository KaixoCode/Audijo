#pragma once
#include "Audijo/pch.hpp"
#include "Audijo/Callback.hpp"

namespace Audijo 
{
	enum Api
	{
		Unspecified, 

#ifdef AUDIJO_ASIO
		Asio,
#endif
#ifdef AUDIJO_WASAPI
		Wasapi
#endif
	};

	enum SampleFormat 
	{
		None,     // Swap   Float  Bytes
		Int8       = 0x00 | 0x00 | 0x01,
		Int16      = 0x00 | 0x00 | 0x02,
		Int32      = 0x00 | 0x00 | 0x04,
		Float32    = 0x00 | 0x10 | 0x04,
		Float64    = 0x00 | 0x10 | 0x08,
		SInt8      = 0x20 | 0x00 | 0x01,
		SInt16     = 0x20 | 0x00 | 0x02,
		SInt32     = 0x20 | 0x00 | 0x04,
		SFloat32   = 0x20 | 0x10 | 0x04,
		SFloat64   = 0x20 | 0x10 | 0x08,

		Swap     = 0x20, // Use like (format & Swap) to determine if it's a byte swapped type
		Floating = 0x10, // Use like (format & Floating) to determine if it's a floating point type
		Bytes    = 0xF,  // Use like (format & Bytes) to determine the amount of bytes in the type
	};

	struct ChannelInfo
	{
		/**
		 * Channel name
		 */
		std::string name;

		/**
		 * Part of group
		 */
		long group;

		/**
		 * Channel is active
		 */
		bool active;

		/**
		 * Channel is input
		 */
		bool input;
	};

	template<Api api = Unspecified>
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
		 * From which api is this device
		 */
		Api api = Unspecified;
	};

	template<typename ...Args>
	DeviceInfo(Args...)->DeviceInfo<Unspecified>;

	enum SettingValues { Default = -2, NoDevice = -1 };

	struct StreamParameters
	{
		/** 
		 * Parameters for the input device 
		 */
		int input = Default;

		/** 
		 * Parameters for the output device 
		 */
		int output = Default;

		/** 
		 * Buffer size 
		 */
		int bufferSize = Default;

		/** 
		 * Sample rate 
		 */
		double sampleRate = Default;

		/**
		 * Allow resampling when chosen samplerate is not available
		 */
		bool resampling = true;
	};

	enum StreamState
	{
		Closed, Opened, Running
	};

	/**
	 * Stream information.
	 */
	struct StreamInformation
	{
		StreamState state = Closed;          // State of the stream	
		int input = NoDevice;                // Input device
		int output = NoDevice;               // output device
		int bufferSize = 0;                  // Buffer size
		double sampleRate = 0;               // Sample rate
		bool resampling = false;             // Eesampling enabled
		int inputChannels = 0;               // Number of input channels
		int outputChannels = 0;              // Number of output channels
		SampleFormat inFormat = None;        // Format used by the callback function for input device
		SampleFormat outFormat = None;       // Format used by the callback function for output device
		SampleFormat deviceInFormat = None;  // Format used by the input device
		SampleFormat deviceOutFormat = None; // Format used by the output device

		StreamInformation& operator=(const StreamParameters& s)
		{
			input = s.input;
			output = s.output;
			bufferSize = s.bufferSize;
			sampleRate = s.sampleRate;
			resampling = s.resampling;
			return *this;
		}
	};

	enum Error
	{
		NoError = 0, // No error
		
		NoMemory,   // Failed to allocate memory
		NotPresent, // Device is not present
		NoApi,      // No Api was specified

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
		virtual const DeviceInfo<>& Device(int id) const = 0;
		virtual int DeviceCount() const = 0;
		virtual const StreamInformation& Information() const { return m_Information;  };

		virtual Error Open(const StreamParameters& settings = StreamParameters{}) = 0;
		virtual Error Start() = 0;
		virtual Error Stop() = 0;
		virtual Error Close() = 0;

		virtual Error SampleRate(double) = 0;

		void Callback(std::unique_ptr<CallbackWrapperBase>&& callback) { m_Callback = std::move(callback); }

		template<typename T>
		void UserData(T& data) { m_UserData = &data; };

	protected:
		static inline double m_SampleRates[]{ 48000, 44100, 88200, 96000, 176400, 192000, 352800, 384000, 8000, 11025, 16000, 22050 };
		
		std::unique_ptr<CallbackWrapperBase> m_Callback;
		void* m_UserData = nullptr;

		StreamInformation m_Information;

		void AllocateBuffers();
		void FreeBuffers();

		void ConvertBuffer(char* outBuffer, char* inBuffer, size_t bufferSize, SampleFormat outFormat, SampleFormat inFormat);
		void ByteSwapBuffer(char* buffer, unsigned int bufferSize, SampleFormat format);

		char** m_InputBuffers = nullptr;
		char** m_OutputBuffers = nullptr;
	};
}