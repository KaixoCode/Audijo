#ifdef AUDIJO_ASIO
#pragma once
#include "Audijo/pch.hpp"
#include "Audijo/ApiBase.hpp"
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
		enum State { Loaded, Initialized, Prepared, Running, Reset };
	public:
		AsioApi(bool loadDevices = true);
		~AsioApi();
		
		const std::vector<DeviceInfo<Asio>>& Devices(bool reload = false);
		const DeviceInfo<>& Device(int id) const override { for (auto& i : m_Devices) if (i.id == id) return i; };
		int DeviceCount() const override { return m_Devices.size(); };
		const DeviceInfo<Asio>& ApiDevice(int id) const { for (auto& i : m_Devices) if (i.id == id) return i; };

		Error Open(const StreamParameters& settings = StreamParameters{}) override;
		Error Start() override;
		Error Stop() override;
		Error Close() override;

		Error SampleRate(double) override;

		Error OpenControlPanel();

	protected:
		std::vector<DeviceInfo<Asio>> m_Devices;

		DeviceInfo<Asio>* DeviceById(int id);

		static void SampleRateDidChange(ASIOSampleRate);
		static long AsioMessage(long, long, void*, double*);
		static ASIOTime* BufferSwitchTimeInfo(ASIOTime*, long, ASIOBool);

		static ASIOCallbacks m_Callbacks;
		static ASIOBufferInfo* m_BufferInfos;
		static AsioApi* m_AsioApi;
		static State m_State;

		friend class DeviceInfo<Asio>;
	};
}
#endif