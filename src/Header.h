#pragma once

#include <OleAuto.h>
#include <comutil.h>
#include "invhelp.h"

class MyComObject
{
public:
	virtual VARIANT __stdcall f(int i) {
		std::wcout << L"f(" << i << L") - data: " << data << std::endl;
		data = i;
		RefData.GetVARIANT().intVal = data;
		RefData.GetVARIANT().vt=VT_INT;
		_variant_t ret(RefData);
		pRefData.pvarVal = &RefData;
		pRefData.vt = VT_VARIANT | VT_BYREF;
		return pRefData;
	}
	virtual BOOL __stdcall g(VARIANT f) {
		std::wcout << L"g(" << f.vt << L")" << std::endl;
		return true;
	}
	virtual BSTR __stdcall s() {
		BSTR bstrStatus = ::SysAllocString(L"MyObject");
		return bstrStatus;
	}
	_variant_t RefData;
	_variant_t pRefData;
	int data;

	MyComObject()  {}
	static PARAMDATA f_paramData;
	static PARAMDATA g_paramData;
	static PARAMDATA s_paramData;
	static METHODDATA methodData[];
	static INTERFACEDATA interfaceData;

	_variant_t init();

};