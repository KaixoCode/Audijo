#pragma once
#include "pch.hpp"
#include "ApiBase.hpp"
#include "asiosys.h"
#include "asio.h"
#include "iasiodrv.h"
#include "asiodrivers.h"

namespace Audijo
{
	template<>
	struct DeviceInfo<Asio> : public DeviceInfo<>
	{
		/**
		 * Get channel information
		 * @param index channel index
		 * @param input is input
		 * @return channel info
		 */
		ChannelInfo& Channel(int index, bool input) const;

		/**
		 * Get list of channel information
		 * @return vector of ChannelInfo
		 */
		std::vector<ChannelInfo>& Channels() const;

	private:
		DeviceInfo(DeviceInfo<>&& d);
		mutable std::vector<ChannelInfo> m_Channels;

		friend class AsioApi;
	};

	class AsioApi : public ApiBase
	{
		enum State { Loaded, Initialized, Prepared, Running };
	public:
		AsioApi();
		
		const std::vector<DeviceInfo<Asio>>& Devices();
		const DeviceInfo<>& Device(int id) override { for (auto& i : m_Devices) if (i.id == id) return i; };
		const DeviceInfo<Asio>& ApiDevice(int id) { for (auto& i : m_Devices) if (i.id == id) return i; };

		Error OpenStream(const StreamParameters& settings = StreamParameters{}) override;
		Error StartStream() override;
		Error StopStream() override;
		Error CloseStream() override;

		Error OpenControlPanel();

	protected:
		std::vector<DeviceInfo<Asio>> m_Devices;

		static void SampleRateDidChange(ASIOSampleRate sRate);
		static long AsioMessage(long selector, long value, void* message, double* opt);
		static ASIOTime* BufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess);

		static ASIOCallbacks m_Callbacks;
		static ASIOBufferInfo* m_BufferInfos;
		static AsioApi* m_AsioApi;
		static State m_State;

		friend class DeviceInfo<Asio>;
	};
}