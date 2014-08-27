#include "stdafx.h"
#include <OleAuto.h>
#include <comutil.h>
#include "CComPtr.h"

class MyObject
{
public:
	virtual void __stdcall f(int i) { std::wcout << L"f(" << i << L")" << std::endl; }
	virtual BOOL __stdcall g(float f) { std::wcout << L"g(" << f << L")" << std::endl; return f>5; }
	static PARAMDATA f_paramData;
	static PARAMDATA g_paramData;
	static METHODDATA methodData[];
	static INTERFACEDATA interfaceData;
};



	PARAMDATA MyObject::f_paramData = {
		OLESTR("i"), VT_I4
	};
	PARAMDATA MyObject::g_paramData = {
		OLESTR("f"), VT_R4
	};
	METHODDATA MyObject::methodData[] = {
			{ OLESTR("f"), &MyObject::f_paramData, 1, 0, CC_STDCALL, 1, DISPATCH_METHOD, VT_EMPTY },
			{ OLESTR("g"), &MyObject::g_paramData, 2, 1, CC_STDCALL, 1, DISPATCH_METHOD, VT_BOOL }
	};
	INTERFACEDATA MyObject::interfaceData = {
		MyObject::methodData,
		sizeof(MyObject::methodData) / sizeof(METHODDATA)
	};

void init() {

	ITypeInfo *pMyobjTypeInfo;
	auto hr = CreateDispTypeInfo(
		&MyObject::interfaceData,
		LOCALE_SYSTEM_DEFAULT,
		&pMyobjTypeInfo);

	IUnknown *pMyobj;

	MyObject myobj;

	hr = CreateStdDispatch(NULL, &myobj, pMyobjTypeInfo, &pMyobj);

	CComPtr<IDispatch> idisp(pMyobj, IID_IDispatch);
	
	_COM_SMARTPTR_TYPEDEF(IDispatch, IID_IDispatch);

	IDispatchPtr idisp2(pMyobj);
	
	
}