#pragma once
#include "pch.hpp"

namespace Audijo 
{
	struct DeviceInfo 
	{
		const unsigned int id;
		const std::string name;
	};

	struct Parameters 
	{
		unsigned int deviceId;
		unsigned int channelAmount;
	};

	class ApiBase 
	{
	protected:
		static inline std::vector<DeviceInfo> m_Devices;
	public:
		virtual void OpenStream(Parameters& in, Parameters& out, unsigned int bufferSize, double sampleRate) {};
		virtual void StartStream() {};
		virtual void StopStream() {};
		virtual void CloseStream() {};
		virtual const std::vector<DeviceInfo>& Devices() = 0;
	};
}