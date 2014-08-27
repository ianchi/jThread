#pragma once

#include <OleAuto.h>
#include <comutil.h>
#include "invhelp.h"

class MyObject
{
public:
	virtual void __stdcall f(int i) {
		std::wcout << L"f(" << i << L")" << std::endl;
	}
	virtual BOOL __stdcall g(float f) {
		std::wcout << L"g(" << f << L")" << std::endl; return f>5;
	}
	virtual BSTR __stdcall s() {
		BSTR bstrStatus = ::SysAllocString(L"MyObject");
		return bstrStatus;
	}

	int data;
	static PARAMDATA f_paramData;
	static PARAMDATA g_paramData;
	static PARAMDATA s_paramData;
	static METHODDATA methodData[];
	static INTERFACEDATA interfaceData;

	_variant_t init();

};