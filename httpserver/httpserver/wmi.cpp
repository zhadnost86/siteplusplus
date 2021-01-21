#include "stdafx.h"
#include "wmi.h"
#define _WIN32_DCOM
#include <windows.h>
#pragma comment(lib, "wbemuuid.lib")

HRESULT hr;

IWbemLocator *locator;
IWbemServices *services;

bool initWmi()
{
	hr =  CoInitializeEx(0, COINIT_MULTITHREADED); 
    if (FAILED(hr)) return false;

	hr = CoCreateInstance(CLSID_WbemLocator, 0, 
        CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &locator);
	if (FAILED(hr)) return false;

	hr = locator->ConnectServer( L"ROOT\\CIMV2", 
		NULL, NULL, NULL, 0, NULL, NULL, &services );
	if (FAILED(hr)) return false;

	hr = CoSetProxyBlanket(
       services,                        // Indicates the proxy to set
       RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
       RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
       NULL,                        // Server principal name 
       RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
       RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
       NULL,                        // client identity
       EOAC_NONE                    // proxy capabilities 
    );

	if (FAILED(hr)) return false;

	return true;
}

bool queryWmi( wchar_t *query, TWMIObjectEnumerator **enumerator )
{
	TWMIObjectEnumerator *res = new TWMIObjectEnumerator();

	hr = services->ExecQuery(
        L"WQL",  query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &res->enm);

	if (FAILED(hr)) return false;

	*enumerator = res;

	return true;
}

HRESULT lasthr()
{
	return hr;
}

bool TWMIObjectEnumerator::getNext()
{
	ULONG ret;

	hr = 0;
	hr = enm->Next(WBEM_INFINITE, 1,  &obj, &ret);

	if ((ret == 0) | FAILED(hr))
		return false;

	return true;
}
