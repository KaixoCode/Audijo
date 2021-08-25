#pragma once
#include "pch.hpp"
#include "ApiBase.hpp"

namespace Audijo
{
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

		enum State { Loaded, Initialized, Prepared, Running };
	public:
		WasapiApi();

		const std::vector<DeviceInfo>& Devices() override;

		Error OpenStream(const StreamSettings& settings = StreamSettings{}) override;
		Error StartStream() override;
		Error StopStream() override;
		Error CloseStream() override;

	protected:
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

		State m_State = Loaded;
	};
}