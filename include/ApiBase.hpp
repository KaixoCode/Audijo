#pragma once
#include "pch.hpp"

namespace Audijo 
{
	enum SampleFormat 
	{
		Int32,
		Float32,
	};

	// TODO: expand when needed
	struct DeviceInfo 
	{
		const int id;
		const std::string name;
		const int inputChannels;
		const int outputChannels;
		const std::vector<double> sampleRates;
		const std::vector<int> bufferSizes;
	};

	struct Parameters 
	{
		int deviceId = -1;
		int channels = -1;
		SampleFormat sampleFormat = Float32;
	};

	struct StreamSettings
	{
		Parameters input{};  // Parameters for the input device
		Parameters output{}; // Parameters for the output device
		int bufferSize = 512;
		double sampleRate = 44100;

	};

	class ApiBase 
	{
	public:
		/**
		 * Search for all available devices.
		 * @return all available devices given the chosen api.
		 */
		virtual const std::vector<DeviceInfo>& Devices() = 0;

		/**
		 * Open the stream.
		 * @param settings StreamSettings
		 */
		virtual void OpenStream(const StreamSettings& settings = StreamSettings{}) = 0;

		/**
		 * Starts the flow of audio through the opened stream. Does nothing
		 * if the stream has not been opened yet.
		 */
		virtual void StartStream() = 0;

		/**
		 * Stop the flow of audio through the stream. Does nothing if the 
		 * stream hasn't been started or opened yet.
		 */
		virtual void StopStream() = 0;

		/**
		 * Close the stream. Also stops the stream if it hasn't been stopped yet.
		 * Does nothing if the stream hasn't been opened yet.
		 */
		virtual void CloseStream() = 0;
	
	protected:
		static inline std::vector<DeviceInfo> m_Devices;
	};
}