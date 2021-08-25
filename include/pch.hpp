



//#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <stdint.h>

#ifndef INITGUID
#define INITGUID
#endif

#include <mfapi.h>
#include <mferror.h>
#include <mfplay.h>
#include <mftransform.h>
#include <wmcodecdsp.h>

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#ifdef _MSC_VER
#pragma comment( lib, "ksuser" )
#pragma comment( lib, "mfplat.lib" )
#pragma comment( lib, "mfuuid.lib" )
#pragma comment( lib, "wmcodecdspuuid" )
#endif

#undef min
#undef max


#include <vector>
#include <cassert>
#include <concepts>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <thread>


#define LOGL(x) std::cout << x << std::endl
#define LOG(x) std::cout << x 

