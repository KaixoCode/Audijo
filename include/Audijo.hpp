#pragma once
#include "pch.hpp"
#include "ApiBase.hpp"
#include "AsioApi.hpp"

namespace Audijo
{
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

		/**
		 * Callback will be moved to a type-erased wrapper inside the ApiBase, where it will be invoked
		 * on request of the Api, and using the selected formats the callback will then be type-casted
		 * back into its original form. The userdata will be moved to the type-erased object as a void*
		 * and when requested to call, will be casted to the correct type inside the type-erased object.
		 */
		template<typename ...Args> requires ValidCallback<int, Args...>
		void SetCallback(Callback<Args...> callback)
		{
			m_Api->SetCallback(std::make_unique<CallbackWrapper<Callback<Args...>, int(Args...)>>(callback));
		};

		template<typename Lambda> requires LambdaConstraint<Lambda>
		void SetCallback(Lambda callback)
		{
			m_Api->SetCallback(std::make_unique<CallbackWrapper<Lambda, typename LambdaSignature<Lambda>::type>>(callback));
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

	//protected:
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

