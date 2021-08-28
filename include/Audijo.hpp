#pragma once
#include "pch.hpp"
#include "ApiBase.hpp"
#include "AsioApi.hpp"
#include "WasapiApi.hpp"

namespace Audijo
{
	/**
	 * Main stream object, with unspecified Api, so it can be dynamically set. To access api specific functions
	 * you need to cast to an api specific Stream object.
	 */
	template<Api api = Unspecified>
	class Stream
	{
	public:
		/**
		 * Constructor
		 * @param api Api
		 */
		Stream(Api api)
		{
			Api(api);
		}

		/**
		 * Constructor
		 */
		Stream()
		{}

		/**
		 * Returns device with the given id.
		 * @param id device id
		 * @return device with id
		 */
		const DeviceInfo<>& Device(int id) const { return m_Api->Device(id); }

		/**
		 * Get the device count.
		 * @return device count
		 */
		int DeviceCount() const { return !m_Api ? 0 : m_Api->DeviceCount(); }

		/**
		 * Get stream information. This call only returns useful information after the stream has been opened.
		 * @return stream information
		 */
		const StreamInformation& Information() const { return m_Api->Information(); }

		/**
		 * Set the callback. A valid callback signare is:
		 * <code>void(Format**, Format**, CallbackInfo, UserObject)</code>
		 * where <code>Format</code> is one of <code>int8_t, int16_t, int32_t, float, double</code>
		 * and <code>UserObject</code> is a reference or a pointer to any type. The UserObject is optional
		 * and can be left out.
		 * @param callback
		 */
		template<typename ...Args> requires ValidCallback<int, Args...>
		void Callback(Callback<Args...> callback)
		{
			if (m_Api) m_Api->Callback(std::make_unique<CallbackWrapper<Callback<Args...>, int(Args...)>>(callback));
		};

		/**
		 * Set the callback. A valid callback signare is:
		 * <code>void(Format**, Format**, CallbackInfo, UserObject)</code>
		 * where <code>Format</code> is one of <code>int8_t, int16_t, int32_t, float, double</code>
		 * and <code>UserObject</code> is a reference or a pointer to any type. The UserObject is optional
		 * and can be left out.
		 * @param callback
		 */
		template<typename Lambda> requires LambdaConstraint<Lambda>
		void Callback(Lambda callback)
		{
			if (m_Api) m_Api->Callback(std::make_unique<CallbackWrapper<Lambda, typename LambdaSignature<Lambda>::type>>(callback));
		};

		/**
		 * Open the stream. The StreamSettings are optional, when they are left out a default device
		 * with a default buffer size and sample rate will be opened.
		 * @param settings <code>StreamSettings</code>
		 * @return 
		 * AlreadyOpen - If the stream is already opened<br>
		 * NoApi - If no Api was specified<br>
		 * InvalidDuplex - If the combination of input and output devices is invalid<br>
		 * Fail - If the device failed to open<br>
		 * NotPresent - If the input/output is not present<br>
		 * InvalidSampleRate - If the sample rate is not supported<br>
		 * NoCallback - If no callback has been set<br>
		 * NoMemory - If it failed to allocate the necessary memory<br>
		 * InvalidBufferSize - If the buffer size is not supported<br>
		 * NoError - If stream started successfully
		 */
		Error Open(const StreamParameters& settings = StreamParameters{}) { return !m_Api ? NoApi : m_Api->Open(settings); };

		/**
		 * Starts the flow of audio through the opened stream. Does nothing
		 * if the stream has not been opened yet.<br>
		 * @return 
		 * NotOpen - If the stream wasn't opened<br>
		 * NoApi - If no Api was specified<br>
		 * AlreadyRunning - If the stream is already running<br>
		 * Fail - If device failed to start<br>
		 * NoError - If stream started successfully
		 */
		Error Start() { return !m_Api ? NoApi : m_Api->Start(); };

		/**
		 * Stop the flow of audio through the stream. Does nothing if the
		 * stream hasn't been started or opened yet.
		 * @return 
		 * NotOpen - If the stream wasn't opened<br>
		 * NoApi - If no Api was specified<br>
		 * NotRunning - If the stream is not running<br>
		 * Fail - If device failed to stop<br>
		 * NoError - If stream stopped successfully
		 */
		Error Stop() { return !m_Api ? NoApi : m_Api->Stop(); };

		/**
		 * Close the stream. Also stops the stream if it hasn't been stopped yet.
		 * Does nothing if the stream hasn't been opened yet.
		 * @return 
		 * NotOpen - If the stream wasn't opened<br>
		 * NoApi - If no Api was specified<br>
		 * Fail - If device failed to close<br>
		 * NoError - If stream stopped successfully
		 */
		Error Close() { return !m_Api ? NoApi : m_Api->Close(); };

		/**
		 * Set the userdata
		 * @param data userdata
		 */
		template<typename T>
		void UserData(T& data) { if (m_Api) m_Api->UserData(data); };

		/**
		 * Set the sample rate of the stream.
		 * @param srate sample rate
		 * @return
		 * NotOpen - If no device is open.
		 * NoApi - If no Api was specified<br>
		 * InvalidSampleRate - If the sample rate is not supported
		 * Fail - If changing the sample rate at this time is not supported, or general fail.
		 * NoError - If sample rate successfully changed
		 */
		Error SetSampleRate(double srate) { return !m_Api ? NoApi : m_Api->SetSampleRate(srate); };

		/**
		 * Get this Stream object as a specific api, to expose api specific functions.
		 * @return this as a stream object for a specific api
		 */
		template<Api api>
		Stream<api>& Get() { return *(Stream<api>*)this; }

		/**
		 * Set the api.
		 * @param api api
		 */
		virtual void Api(Api api)
		{
			m_Type = api;
			switch (api)
			{
			case Asio: m_Api = std::make_unique<AsioApi>(); break;
			case Wasapi: m_Api = std::make_unique<WasapiApi>(); break;
			default: throw std::exception("Incompatible api");
			}
		}

		/**
		 * Get the current api of this stream.
		 * @return api
		 */
		virtual Audijo::Api Api() const { return m_Type; }

	protected:
		std::unique_ptr<ApiBase> m_Api = nullptr;
		Audijo::Api m_Type = Unspecified;
	};

	/**
	 * Wasapi specific Stream object, for when api is decided at compiletime, 
	 * exposes api specific functions directly.
	 */
	template<>
	class Stream<Wasapi> : public Stream<>
	{
		// Delete methods
		void Api(Audijo::Api api) override {};
		using Stream<>::Get;

	public:
		Stream()
			: Stream<>(Wasapi)
		{}

		/**
		 * Search for all available devices. When called more than once, the list will be updated.
		 * @return all available devices given the chosen api.
		 */
		const std::vector<DeviceInfo<Wasapi>>& Devices() const { return ((WasapiApi*)m_Api.get())->Devices(); }

		/**
		 * Returns device with the given id.
		 * @param id device id
		 * @return device with id
		 */
		const DeviceInfo<Wasapi>& Device(int id) const { return ((WasapiApi*)m_Api.get())->ApiDevice(id); }

		virtual Audijo::Api Api() const override { return Wasapi; };
	};

	/**
	 * Asio specific Stream object, for when api is decided at compiletime,
	 * exposes api specific functions directly.
	 */
	template<>
	class Stream<Asio> : public Stream<>
	{
		// Delete the api method
		void Api(Audijo::Api api) override {};
	
	public:
		Stream()
			: Stream<>(Asio)
		{}

		/**
		 * Opens the ASIO control panel.
		 * @return
		 * NotOpen - if no device has been opened yet<br>
		 * NoError - On success
		 */
		Error OpenControlPanel() { return ((AsioApi*)m_Api.get())->OpenControlPanel(); }

		/**
		 * Search for all available devices. When called more than once, the list will be updated.
		 * @return all available devices given the chosen api.
		 */
		const std::vector<DeviceInfo<Asio>>& Devices() const { return ((AsioApi*)m_Api.get())->Devices(); }

		/**
		 * Returns device with the given id.
		 * @param id device id
		 * @return device with id
		 */
		const DeviceInfo<Asio>& Device(int id) const { return ((AsioApi*)m_Api.get())->ApiDevice(id); }
	
		virtual Audijo::Api Api() const override { return Asio; };
	};

	Stream(Api)->Stream<Unspecified>;
	Stream()->Stream<Unspecified>;
}

