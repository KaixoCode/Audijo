#pragma once
#include "pch.hpp"
#include "ApiBase.hpp"
#include "AsioApi.hpp"

/*
Audijo()
{
	drivers.removeCurrentDriver();
	for (int i = 0; i < drivers.asioGetNumDev(); i++)
	{
		DriverInfo& info = m_Drivers.emplace_back();
		char* name = new char[50];
		drivers.asioGetDriverName(i, name, 50);
		info.name = name;
		info.id = i;
		delete name;
		LOGL(info.name);
	}
	drivers.asioOpenDriver(m_Drivers[0].id, (void**)&theAsioDriver);

	driverInfo.asioVersion = 2;
	driverInfo.sysRef = GetForegroundWindow();

	ASIOError result = ASIOInit(&driverInfo);
	if (result != ASE_OK) {
		LOGL(" error (" << getAsioErrorString(result) << ") stopping device.");
	}
	LOGL("" << driverInfo.asioVersion);
	LOGL("" << driverInfo.driverVersion);
	LOGL("" << driverInfo.errorMessage);
	LOGL("" << driverInfo.name);
	LOGL("" << driverInfo.sysRef);
}
*/


namespace Audijo
{
	enum Api 
	{
		UNSPECIFIED, ASIO, WASAPI
	};

	template<Api api = UNSPECIFIED>
	struct Stream;

	Stream(Api)->Stream<UNSPECIFIED>;

	struct StreamBase 
	{ 
		StreamBase(Api api = ASIO) 
		{
			switch (api) {
			case ASIO: m_Api = std::make_unique<AsioApi>(); break;
			default:
				throw std::exception("Incompatible api");
			}
		}

		const std::vector<DeviceInfo>& Devices() { return m_Api->Devices(); }

		std::unique_ptr<ApiBase> m_Api;
	};

	template<>
	struct Stream<UNSPECIFIED> : public StreamBase
	{
		Stream(Api api = ASIO)
			: StreamBase(api)
		{}
	};

	template<>
	struct Stream<WASAPI> : public StreamBase
	{
		Stream()
			: StreamBase(WASAPI)
		{}
	};

	template<>
	struct Stream<ASIO> : public StreamBase
	{
		Stream()
			: StreamBase(ASIO)
		{}
	};

}

