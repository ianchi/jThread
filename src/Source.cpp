#include "stdafx.h"
#include "Header.h"
#include "invhelp.h"

	PARAMDATA MyComObject::f_paramData = {OLESTR("i"), VT_I4};
	PARAMDATA MyComObject::g_paramData = { OLESTR("f"), VT_VARIANT };
	PARAMDATA MyComObject::s_paramData = { OLESTR(""), VT_EMPTY };
	METHODDATA MyComObject::methodData[] = {
			{ OLESTR("byRef"), &MyComObject::f_paramData, 1, 0, CC_STDCALL, 1, DISPATCH_METHOD, VT_VARIANT },
			{ OLESTR("g"), &MyComObject::g_paramData, 2, 1, CC_STDCALL, 1, DISPATCH_METHOD, VT_BOOL },
			{ OLESTR("toString"), &MyComObject::s_paramData, 3, 2, CC_STDCALL, 0, DISPATCH_METHOD, VT_BSTR }
	};
	INTERFACEDATA MyComObject::interfaceData = {
		MyComObject::methodData,
		sizeof(MyComObject::methodData) / sizeof(METHODDATA)
	};

	_variant_t  MyComObject::init() {

	ITypeInfo *pMyobjTypeInfo;
	auto hr = CreateDispTypeInfo(
		&MyComObject::interfaceData,
		LOCALE_SYSTEM_DEFAULT,
		&pMyobjTypeInfo);

	IUnknown *pMyobj;


	hr = CreateStdDispatch(NULL, this, pMyobjTypeInfo, &pMyobj);

	_COM_SMARTPTR_TYPEDEF(IDispatch, IID_IDispatch);

	IDispatchPtr idisp2(pMyobj);
	VARIANT ret;
	////auto ret = idisp2->Invoke();
	//Invoke(idisp2, DISPATCH_METHOD, &ret, NULL, NULL, L"g", L"R", 12.3);
	Invoke(idisp2, DISPATCH_METHOD, &ret, NULL, NULL, L"byRef", L"I", 5);
	_variant_t variant(idisp2.GetInterfacePtr());
	
	return variant;
	

	
}