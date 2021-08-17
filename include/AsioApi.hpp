#pragma once
#include "pch.hpp"
#include "ApiBase.hpp"
#include "asiosys.h"
#include "asio.h"
#include "iasiodrv.h"
#include "asiodrivers.h"

namespace Audijo
{
	class AsioApi : public ApiBase
	{
		enum State { Loaded, Initialized, Prepared, Running };
	public:

		const std::vector<DeviceInfo>& Devices() override;

		Error OpenStream(const StreamSettings& settings = StreamSettings{}) override;
		Error StartStream() override;
		Error StopStream() override;
		Error CloseStream() override;

		void OpenControlPanel() { ASIOControlPanel(); }

	protected:
		StreamSettings m_Settings;
		
		static void SampleRateDidChange(ASIOSampleRate sRate);
		static long AsioMessage(long selector, long value, void* message, double* opt);
		static ASIOTime* BufferSwitchTimeInfo(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess);

		static ASIOCallbacks m_Callbacks;
		static ASIOBufferInfo* m_BufferInfos;
		static AsioApi* m_AsioApi;
		static State m_State;
	};
}