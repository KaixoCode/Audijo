#pragma once
#include "pch.hpp"


struct Audijo
{
	Audijo()
	{
		ASIODriverInfo info;
		info.asioVersion = 2;

		ASIOInit(&info);

		LOGL("" << info.asioVersion);
		LOGL("" << info.driverVersion);
		LOGL("" << info.errorMessage);
		LOGL("" << info.name);
		LOGL("" << info.sysRef);

		ASIOExit();
	}
};