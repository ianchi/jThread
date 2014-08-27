#include "stdafx.h"
#include "Header.h"



	PARAMDATA MyObject::f_paramData = {OLESTR("i"), VT_I4};
	PARAMDATA MyObject::g_paramData = {OLESTR("f"), VT_R4};
	PARAMDATA MyObject::s_paramData = { OLESTR(""), VT_EMPTY };
	METHODDATA MyObject::methodData[] = {
			{ OLESTR("f"), &MyObject::f_paramData, 1, 0, CC_STDCALL, 1, DISPATCH_METHOD, VT_EMPTY },
			{ OLESTR("g"), &MyObject::g_paramData, 2, 1, CC_STDCALL, 1, DISPATCH_METHOD, VT_BOOL },
			{ OLESTR("toString"), &MyObject::s_paramData, 3, 2, CC_STDCALL, 0, DISPATCH_METHOD, VT_BSTR }
	};
	INTERFACEDATA MyObject::interfaceData = {
		MyObject::methodData,
		sizeof(MyObject::methodData) / sizeof(METHODDATA)
	};

	_variant_t  MyObject::init() {

	ITypeInfo *pMyobjTypeInfo;
	auto hr = CreateDispTypeInfo(
		&MyObject::interfaceData,
		LOCALE_SYSTEM_DEFAULT,
		&pMyobjTypeInfo);

	IUnknown *pMyobj;


	hr = CreateStdDispatch(NULL, this, pMyobjTypeInfo, &pMyobj);

	_COM_SMARTPTR_TYPEDEF(IDispatch, IID_IDispatch);

	IDispatchPtr idisp2(pMyobj);
	//VARIANT ret;
	////auto ret = idisp2->Invoke();
	//Invoke(idisp2, DISPATCH_METHOD, &ret, NULL, NULL, L"g", L"R", 12.3);
	//Invoke(idisp2, DISPATCH_METHOD, NULL, NULL, NULL, L"f", L"I", 5);
	_variant_t variant(idisp2.GetInterfacePtr());
	
	return variant;
	

	
}