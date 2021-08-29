#ifdef AUDIJO_WASAPI
#pragma once
#include "Audijo/pch.hpp"
#include "Audijo/ApiBase.hpp"

namespace Audijo
{
	template<>
	struct DeviceInfo<Wasapi> : public DeviceInfo<>
	{

	private:
		DeviceInfo(DeviceInfo<>&& d)
			: DeviceInfo<>{ std::forward<DeviceInfo<>>(d) }
		{}

		friend class WasapiApi;
	};

	class WasapiApi : public ApiBase
	{
		template<typename T>
		static void SafeRelease(T* obj)
		{
			if constexpr (std::is_base_of_v<IUnknown, T>)
			{
				if (obj)
					obj->Release();
				obj = nullptr;
			}
			else if constexpr (std::is_same_v<PROPVARIANT, T>)
			{
				PropVariantClear(obj);
				delete obj;
			}
			else CoTaskMemFree(obj);
		}

		// Pointer wrapper because windows api code sucks arse and 
		// makes you clean up their mess yourself...
		template<typename T>
		struct Pointer
		{
			Pointer() : ptr(nullptr) {}
			Pointer(T* t) : ptr(t) {}
			~Pointer() { Release(); }
			void Release() { SafeRelease(ptr); ptr = nullptr; }

			void operator=(T* t) { ptr = t; };
			bool operator==(T* t) { return ptr == t; };
			T* operator->() { return ptr; }
			T** operator&() { return &ptr; }
			T* get() { return ptr; }
			operator T*() { return ptr; }
			operator bool() { return ptr != nullptr; };
		
		private:
			T* ptr;
		};

	public:
		WasapiApi();
		~WasapiApi() { Close(); }

		const std::vector<DeviceInfo<Wasapi>>& Devices();
		const DeviceInfo<>& Device(int id) const override { for (auto& i : m_Devices) if (i.id == id) return (DeviceInfo<>&)i; };
		int DeviceCount() const override { return m_Devices.size(); };
		const DeviceInfo<Wasapi>& ApiDevice(int id) const { for (auto& i : m_Devices) if (i.id == id) return i; };

		Error Open(const StreamParameters& settings = StreamParameters{}) override;
		Error Start() override;
		Error Stop() override;
		Error Close() override;

		Error SampleRate(double) override;

	protected:
		std::vector<DeviceInfo<Wasapi>> m_Devices;

		bool m_CoInitialized = false;
		Pointer<IMMDeviceEnumerator> m_DeviceEnumerator; // The wasapi device enumerator
		Pointer<IMMDeviceCollection> m_WasapiDevices; // Device collection
		Pointer<IMMDevice> m_InputDevice;  // Input device
		Pointer<IMMDevice> m_OutputDevice; // Output device
		Pointer<IAudioClient> m_InputClient;  // General client for input
		Pointer<IAudioClient> m_OutputClient; // General client for output
		Pointer<IAudioCaptureClient> m_CaptureClient; // Input client
		Pointer<IAudioRenderClient> m_RenderClient;   // Output client

		std::thread m_AudioThread;
	};
}
#endif