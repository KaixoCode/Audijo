#pragma once
#include "pch.hpp"
#include "Callback.hpp"

namespace Audijo 
{
	struct DeviceInfo 
	{
		const int id;
		const std::string name;
		const int inputChannels;
		const int outputChannels;
		const std::vector<double> sampleRates;
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
	};

	enum Error
	{
		NoError = 0,
		
		NoMemory,   // Failed to allocate memory
		NotPresent, // Device is not present

		InvalidSampleRate, // Device does not support sample rate
		InvalidBufferSize, // Device does not support buffer size

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
		static inline std::vector<DeviceInfo> m_Devices;
		static inline double m_SampleRates[]{ 48000, 44100, 88200, 96000, 176400, 192000, 352800, 384000, 8000, 11025, 16000, 22050 };
		std::unique_ptr<CallbackWrapperBase> m_Callback;
		void* m_UserData = nullptr;
	};
}