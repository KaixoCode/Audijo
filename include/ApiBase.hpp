#pragma once
#include "pch.hpp"

namespace Audijo 
{
	struct DeviceInfo 
	{
		const int id;
		const std::string name;

		DeviceInfo(int id, std::string name) : id(id), name(name) 
		{

		}
	};

	class ApiBase 
	{
	protected:
		static std::vector<DeviceInfo> m_Devices;
	public:
		virtual void OpenStream(DeviceInfo& inputDevice, DeviceInfo& outputDevice, unsigned int bufferSize, unsigned int channels) = 0;
		virtual void StartStream() = 0;
		virtual void StopStream() = 0;
		virtual void CloseStream() = 0;
	};
}