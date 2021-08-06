#pragma once
#include "pch.hpp"
#include "asiosys.h"
#include "asio.h"
#define interface struct
#include "iasiodrv.h"
#include "asiodrivers.h"


static AsioDrivers drivers;
static ASIODriverInfo driverInfo;
extern IASIO* theAsioDriver;

static const char* getAsioErrorString(ASIOError result)
{
	struct Messages
	{
		ASIOError value;
		const char* message;
	};

	static const Messages m[] =
	{
	  {   ASE_NotPresent,    "Hardware input or output is not present or available." },
	  {   ASE_HWMalfunction,  "Hardware is malfunctioning." },
	  {   ASE_InvalidParameter, "Invalid input parameter." },
	  {   ASE_InvalidMode,      "Invalid mode." },
	  {   ASE_SPNotAdvancing,     "Sample position not advancing." },
	  {   ASE_NoClock,            "Sample clock or rate cannot be determined or is not present." },
	  {   ASE_NoMemory,           "Not enough memory to complete the request." }
	};

	for (unsigned int i = 0; i < sizeof(m) / sizeof(m[0]); ++i)
		if (m[i].value == result) return m[i].message;

	return "Unknown error.";
}

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

/*
 * Options for opening a stream:
 *  - A stream base class that you extend, with a callback
 *  - Simply a struct of device information, and then call an 
 *    OpenStream method with that info and a pointer to a callback function
 *  
 * 
 */

namespace Audijo
{
	struct DriverInfo
	{
		unsigned int id;
		std::string name;
	};

	std::vector<DriverInfo> m_Drivers;


	enum Api 
	{
		UNSPECIFIED, ASIO, WASAPI
	};

	template<Api api = UNSPECIFIED>
	struct Stream;

	Stream()->Stream<UNSPECIFIED>;

	template<>
	struct Stream<UNSPECIFIED> 
	{
		Stream(Api api = ASIO)
		{}
	};

	template<>
	struct Stream<WASAPI>
	{

	};

	template<>
	struct Stream<ASIO>
	{

	};

}

