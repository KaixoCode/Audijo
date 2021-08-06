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
	template<typename InFormat, typename OutFormat, typename UserData>
	using Callback = int(*)(InFormat, OutFormat, UserData&);

	enum Api 
	{
		Unspecified, Asio, Wasapi
	};

	template<Api api = Unspecified>
	class Stream 
	{
	public:
		/**
		 * Constructor
		 * @param api audio api
		 */
		Stream(Api api)
		{
			switch (api)
			{
			case Asio: m_Api = std::make_unique<AsioApi>(); break;
			default: throw std::exception("Incompatible api");
			}
		}

		/**
		 * Search for all available devices.
		 * @return all available devices given the chosen api.
		 */
		const std::vector<DeviceInfo>& Devices() { return m_Api->Devices(); }


		template<typename InFormat, typename OutFormat, typename UserData>
		void SetCallback(Callback<InFormat, OutFormat, UserData> callback) 
		{

		};

		/**
		 * Open the stream.
		 * @param settings StreamSettings
		 */
		void OpenStream(const StreamSettings& settings = StreamSettings{}) { m_Api->OpenStream(settings); };

		/**
		 * Starts the flow of audio through the opened stream. Does nothing
		 * if the stream has not been opened yet.
		 */
		void StartStream() { m_Api->StartStream(); };

		/**
		 * Stop the flow of audio through the stream. Does nothing if the
		 * stream hasn't been started or opened yet.
		 */
		void StopStream() { m_Api->StopStream(); };

		/**
		 * Close the stream. Also stops the stream if it hasn't been stopped yet.
		 * Does nothing if the stream hasn't been opened yet.
		 */
		void CloseStream() { m_Api->CloseStream(); };

	protected:
		std::unique_ptr<ApiBase> m_Api;
	};

	template<>
	class Stream<Wasapi> : public Stream<>
	{
	public:
		Stream()
			: Stream<>(Wasapi)
		{}
	};

	template<>
	class Stream<Asio> : public Stream<>
	{
	public:
		Stream()
			: Stream<>(Asio)
		{}

		/**
		 * TODO: Opens the ASIO control panel.
		 */
		void OpenControlPanel() {}
	};

	Stream(Api)->Stream<Unspecified>;
}

