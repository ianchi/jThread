#include "stdafx.h"
#include "RuntimeThread.h"
#include "ExecProcess.h"

using namespace std;

RuntimeThread::~RuntimeThread()
{
}

void RuntimeThread::startThread() {
	this->futureReturn = std::async(launch::async,&RuntimeThread::runThread, this);

}
//main thread for the new runtime
void RuntimeThread::runThread() {
	unique_lock<timed_mutex> lockM(runningMutex, std::defer_lock); //block until end of function for cbWait. 

	start(L" - Start script " + strName);

	try
	{
		if (!runtime.is_valid())
			runtime = jsrt::runtime::create(JsRuntimeAttributeNone, JsRuntimeVersionEdge);
		if (!context.is_valid()) {
			context = runtime.create_context();// hostInfo->debugApplication);
		}

		if (!strScript.empty()) {
			jsrt::context::scope scope(context);
			createjThreadProxy();

			//if (debugBreak && hostInfo->debugApplication != nullptr) hostInfo->debugApplication->CauseBreak();
			if(profile) startProfiling();

			state = (DWORD)stateEnum::running;
			returnValue << runScript(strScript);
			if (!(state & stateEnum::terminated)) {
				state = (DWORD)stateEnum::idle;
				end(L" - End script " + strName);
			}

			if (profile) {
				context.stop_profiling(0);
				profiler->log();
			}
		}
	}
	catch (...){
		raise(L"jThread - Fatal error.\n");
		state = (DWORD)stateEnum::terminated;
		end(L" - Abnormal script termination " + strName);
	}
	

	//wait for all children to finish
	for (auto child = children.begin(); child != children.end(); ++child) {
		(*child)->wait(INFINITE);
	}
	if (!(state & stateEnum::terminated)) state = (DWORD)stateEnum::ended;

}

//creates js jThread object 
void RuntimeThread::createjThreadProxy() {
	jThread = make_unique<jsrt::proxy_object<RuntimeThread>>(*this);

	context.global().createValueProp(L"jThread", *(static_cast<jsrt::value *>(jThread.get())));

	jThread->createValueProp(L"version", hostInfo->version);
	jThread->createValueProp(L"name", hostInfo->name);
	jThread->createValueProp(L"path", hostInfo->path);

	jThread->createValueProp(L"scriptName", strName);
	jThread->createValueProp(L"scriptPath", strPath);
	jThread->createValueProp(L"scriptFullName", strPath + L"\\" + strName);
	jThread->createValueProp(L"arguments", strArguments);
	jThread->createValueProp(L"isAsync",outputMode & outputModeEnum::batchOut ? true : false);

	jThread->bindAccesor<wstring>(L"stdOut", &RuntimeThread::stdOut, nullptr);
	jThread->bindAccesor<wstring>(L"stdErr", &RuntimeThread::stdErr, nullptr);

	jThread->bindAccesor(L"currentFolder", &RuntimeThread::cbGetCurrentFolder, &RuntimeThread::cbSetCurrentFolder);

	jThread->bindMethod(L"echo", &RuntimeThread::echo);
	jThread->bindMethod(L"error", &RuntimeThread::error);
	jThread->bindMethod(L"input", &RuntimeThread::cbGetInput);
	jThread->bindMethod(L"sleep", &RuntimeThread::cbSleep);
	jThread->bindMethod(L"getUserName", &RuntimeThread::cbGetUserName);

	jThread->bindMethod(L"loadModule", &RuntimeThread::cbLoadModule);
	jThread->bindMethod(L"runScriptAsync", &RuntimeThread::cbRunScriptAsync);
	jThread->bindMethod(L"evalAsync", &RuntimeThread::cbEvalAsync);
	jThread->bindMethod(L"functionAsync", &RuntimeThread::cbFunctionAsync);

	jThread->bindMethod(L"exec", &RuntimeThread::newProcess);

	jThread->bindMethod(L"createObject", &RuntimeThread::cbCreateObject);
	jThread->bindMethod(L"invoke", &RuntimeThread::cbInvoke);

	context.global().createValueProp(L"ActiveXObject", *(static_cast<jsrt::value *>(&(jsrt::function<jsrt::value, wstring>::create(cbActiveXObject)))));

	jThread->bindMethod(L"copyFile", &RuntimeThread::cbCopyFile);
	jThread->bindMethod(L"deleteFile", &RuntimeThread::cbDeleteFile);
	jThread->bindMethod(L"moveFile", &RuntimeThread::cbMoveFile);
	jThread->bindMethod(L"getFile", &RuntimeThread::cbGetFile);
	jThread->bindMethod(L"getAbsolutePath", &RuntimeThread::cbGetFullPathName);


	jThread->prevent_extension();

}

void RuntimeThread::terminate() {
	
	//terminate all subprocesses
	for (auto child = children.begin(); child != children.end(); child++)
		(*child)->terminate();
	
	//stop current script
	runtime.disable_execution();
	state = (DWORD)stateEnum::terminated;
	end(L" - Abnormal script termination " + strName);

}
//creates js async object proxy in the parent thread
jsrt::value RuntimeThread::createChildProxy() {
	
	//must be called by the parent thread to create object in the corresponding runtime
	proxyInParent = make_unique<jsrt::proxy_object<RuntimeThread>>(*this);

	proxyInParent->bindAccesor<wstring>(L"stdOut", &RuntimeThread::stdOut, nullptr);
	proxyInParent->bindAccesor<wstring>(L"stdErr", &RuntimeThread::stdErr, nullptr);
	proxyInParent->bindAccesor<double>(L"duration", &RuntimeThread::getDuration, nullptr);
	proxyInParent->bindProperty(L"state", &state);
	proxyInParent->bindAccesor<jsrt::value>(L"returnValue", &RuntimeThread::cbGetReturnValue, nullptr);

	proxyInParent->bindMethod(L"wait", &RuntimeThread::cbWait);
	proxyInParent->bindMethod(L"terminate", &RuntimeThread::terminate);

	proxyInParent->prevent_extension();

	return *(static_cast<jsrt::value *>(proxyInParent.get()));
}

bool RuntimeThread::cbWait(jsrt::optional<DWORD> milliseconds) {
	unique_lock<timed_mutex> lockM(runningMutex, std::defer_lock);

	bool result=true; 
	if (milliseconds.has_value() && milliseconds.value()!=INFINITE)
		result = lockM.try_lock_for(chrono::milliseconds(max(milliseconds.value(), 0)));
	else
		lockM.lock();
	return result;
}

//loads an external js file, and runs it in the current context, sharing the global scope (and thus no parameters are passed)
void RuntimeThread::cbLoadModule(wstring scriptPath) {
	wstring script, name = getFilename(scriptPath);
	jsrt::value scriptResult;

	if (!readFile(scriptPath, script)) {
		raise(L"jThread - " + script);
		script = L"";
	}
	else {
		runScript(script,name);
	}
}

//loads an external js file and runs it in a new independant runtime asynchronously
jsrt::value RuntimeThread::cbRunScriptAsync(wstring scriptFilename, const jsrt::rest<jsrt::value> arguments)
{
	vector<wstring> args;

	//serialize arguments and start new thread
	for (UINT i = 1; i < arguments.size(); i++) 
		args.push_back(jsonStringify(arguments[i]));
	
	return newThread(scriptFilename, nullptr, &args);
}

//loads an external js file and runs it in a new independant runtime asynchronously
jsrt::value RuntimeThread::cbEvalAsync(wstring script, const jsrt::rest<jsrt::value> arguments)
{
	vector<wstring> args;

	//serialize arguments and start new thread
	for (UINT i = 1; i < arguments.size(); i++)
		args.push_back(jsonStringify(arguments[i]));

	return newThread(L"AsyncEval", &script, &args);
}

//loads an external js file and runs it in a new independant runtime asynchronously
jsrt::value RuntimeThread::cbFunctionAsync(jsrt::value func, const jsrt::rest<jsrt::value> arguments)
{
	wstring strArgs;
	vector<wstring> args;
	//extracts code from function
	if (func.type() != JsValueType::JsFunction)
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"first argument must be a function"));
		return jsrt::object();
	}
	jsrt::object jsFunction(func);
	jsrt::function<wstring> jsToString(jsFunction.get_property<jsrt::object>(jsrt::property_id::create(L"toString")));
	wstring script = jsToString(jsFunction);
	//extracts function name
	wsmatch	res;
	regex_search(script, res, wregex(L"^ *function *([^( ]*)", regex::ECMAScript));
	//if it is an anonymous function, asign it to be able to call it

	wstring scriptName = res[1];
	if (scriptName == L"") {
		script = L"var anonymous= " + script;
		scriptName = L"anonymous";
	}

	//serialize arguments and start new thread
	wstring obj;
	for (UINT i = 1; i < arguments.size(); i++) {
		obj = jsonStringify(arguments[i]);
		args.push_back(obj);
		strArgs += (strArgs.empty() ? L"" : L", ") + obj;
	}
	
	script += L"\n var result= " + scriptName + L"(" + strArgs + L");\nresult;";

	return newThread(L"AsyncFunction " + scriptName, &script, &args);
}
//runs a script in the current context
wstring RuntimeThread::runScript(const wstring &script, wstring name) {
	jsrt::value result;
	wstring scriptResult;
	static JsSourceContext id{ 0 };
	id++; //change id on each call to identify source name in call-stack trace
	name = name.empty() ? strName : name;
	try {

		auto funcScript=context.parse(script, id, name);
		
		if (hostInfo->debugApplication != nullptr) startDebugging();

		result = funcScript(context.global(), {});

		//result = context.evaluate(script, id, name);
		scriptResult = jsonStringify(result);
	}
	catch (const jsrt::script_exception &e) {
		error(L"Error in " + name + L": " + e.what() + L"\n");
	}
	catch (const jsrt::script_compile_exception &e) {
		error(L"Error in " + name + L" " + e.what() + L"\n");
	}
	catch (const jsrt::exception &e) {
		error(L"Error in " + name + L" " + e.what() + L"\n");
	}

	return scriptResult;
}

//Spawns an async js script
jsrt::value RuntimeThread::newThread(const wstring &filename, wstring *script, const vector<wstring> *arguments) {
	auto rtm = static_cast<RuntimeThread*>(addChild(make_unique<RuntimeThread>(this,filename, script, arguments)));

	rtm->startThread();

	//create proxy object to child thread in the current runtime
	return rtm->createChildProxy();
}

//Spawns a new windows process
jsrt::value RuntimeThread::newProcess(wstring command, wstring arguments) {
	auto proc = static_cast<ExecProcess*>(addChild(make_unique<ExecProcess>(static_cast<basic_process*>(this), command, arguments)));

	proc->startThread();

	//create proxy object to child thread in the current runtime
	return proc->createChildProxy();
}
//exposes JScript JSON Stringify function
std::wstring RuntimeThread::jsonStringify(jsrt::value param) {
	jsrt::object JSON = jsrt::context::global().get_property<jsrt::object>(jsrt::property_id::create(L"JSON"));
	jsrt::function<std::wstring, jsrt::value> stringify(JSON.get_property<jsrt::object>(jsrt::property_id::create(L"stringify")));
	return stringify(JSON, param);
}

//exposes JScript JSON Parse function
jsrt::value RuntimeThread::jsonParse(std::wstring param) {
	jsrt::object JSON = jsrt::context::global().get_property <jsrt::object>(jsrt::property_id::create(L"JSON"));
	jsrt::function<jsrt::value, std::wstring> parse(JSON.get_property <jsrt::value>(jsrt::property_id::create(L"parse")));
	return (param == L"undefined") ? jsrt::context::undefined() : parse(JSON, param);
}


//
// Method to start up debugging in the current context.
//

void RuntimeThread::startDebugging()
{
	HRESULT hr = S_OK;
	IClassFactory *classFactory = nullptr;
	IProcessDebugManager *pdm = nullptr;
	IDebugApplication *debugApplication = nullptr;

	// Initialize COM because we're going to have to talk to some COM interfaces.
	if (CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED) != S_OK) goto error;

	// Get a pointer to the process debug manager (PDM).
	if (CoGetClassObject(__uuidof(ProcessDebugManager), CLSCTX_INPROC_SERVER, NULL, __uuidof(IClassFactory), (LPVOID *)&classFactory) != S_OK)
		goto error;
	if (classFactory->CreateInstance(0, _uuidof(IProcessDebugManager), (LPVOID *)&pdm)  != S_OK)
		goto error;

	// Get the default application.
	if (pdm->GetDefaultApplication(&debugApplication) != S_OK)
		goto error;


	auto ret1=debugApplication->FCanJitDebug();
	wcout << L"FCanJitDebug: " << ret1 << endl;
	auto ret2 = debugApplication->FIsAutoJitDebugEnabled();
	wcout << L"FIsAutoJitDebugEnabled: " << ret2 << endl; {}

	if(debugBreak) debugApplication->CauseBreak();
	
//	wcout << L"CauseBreak: " << ret4 << endl;

	context.start_debugging(debugApplication);

	//auto ret3=debugApplication->StartDebugSession();
	//wcout << L"StargDebug: " << ret3 << endl;





error:
	if (debugApplication)
		debugApplication->Release();

	if (pdm)
		pdm->Release();

	if (classFactory)
		classFactory->Release();

	if (FAILED(hr))
	{
		fwprintf(stderr, L"JSHost: couldn't start debugging.\n");
	}

	else fwprintf(stderr, L"debugger set.\n");
}




void RuntimeThread::startProfiling() {

	if (profile)
	{
		if(profiler==nullptr) profiler = new Profiler();
		IActiveScriptProfilerCallback *callback;

		profiler->QueryInterface(IID_IActiveScriptProfilerCallback, (void **)&callback);
		profiler->Release();

		context.start_profiling(callback, PROFILER_EVENT_MASK_TRACE_ALL, 0);
		callback->Release();
	}


}

jsrt::value RuntimeThread::cbActiveXObject(const jsrt::call_info &info, std::wstring progID){
	IDispatch *obj;
	jsrt::value ret = jsrt::context::undefined();

	CoInitialize(NULL);
	//can only be created with the "new" keyword
	if (!info.is_construct_call())
		jsrt::context::set_exception(jsrt::error::create(L"Object doesn't support this action"));

	else switch (CreateObject(progID, &obj)) {
	case NOERROR:
		{_variant_t var(obj, false);
		ret = jsrt::value::from_variant(&var);
		}
		break;
	case REGDB_E_CLASSNOTREG:
	case CO_E_CLASSSTRING:
		jsrt::context::set_exception(jsrt::error::create(L"Invalid ActiveX Class string."));
		break;
	default:
		jsrt::context::set_exception(jsrt::error::create(L"Error creating ActiveXObject."));

	}

	return ret;
}

jsrt::value RuntimeThread::cbInvoke(jsrt::value obj, std::wstring method, jsrt::rest<jsrt::value> params) {
	jsrt::value ret = context.undefined();
	_variant_t comObj;
	//check parameters
	obj.to_variant(&comObj);
	if (comObj.vt!= VT_DISPATCH)
		jsrt::context::set_exception(jsrt::error::create_type_error(L"First parameter must be an ActiveX object."));
	else {
		DISPID dispid;
		HRESULT hr;
		LPOLESTR str = (LPOLESTR)method.c_str();

		// Get DISPID of property/method 
		hr = comObj.pdispVal->GetIDsOfNames(IID_NULL, &str, 1, LOCALE_USER_DEFAULT, &dispid);
		if (FAILED(hr))
			jsrt::context::set_exception(jsrt::error::create(L"Object doesn't support method."));


		DISPPARAMS dispparams;
		memset(&dispparams, 0, sizeof dispparams);

		dispparams.cArgs = params.size();
		jsrt::property_id byRef = jsrt::property_id::create(L"byRef");

		VARIANTARG* pvarg = NULL;
		vector<pair<unique_ptr<_variant_t>, jsrt::object>> byRefs;
		if (dispparams.cArgs != 0)
		{
			// allocate memory for all VARIANTARG parameters 
			pvarg = new VARIANTARG[dispparams.cArgs];
			if (pvarg == NULL) {
				jsrt::context::set_exception(jsrt::error::create(L"Not enough memory."));
				return ret;
			}
			else {
				dispparams.rgvarg = pvarg;
				memset(pvarg, 0, sizeof(VARIANTARG) * dispparams.cArgs);

				pvarg += dispparams.cArgs - 1; // params go in opposite order
				for (auto param = params.begin(); param != params.end(); param++, pvarg--) {
					//check if we need to pass by reference
					if (param->type() == JsObject) {
						jsrt::object obj(*param);
						if (obj.has_property(byRef)) {
							unique_ptr<_variant_t> pVar = make_unique<_variant_t>();
							obj.get_property(byRef).to_variant(pVar.get());
							pvarg->vt = VT_VARIANT | VT_BYREF;
							pvarg->pvarVal = pVar.get();
							byRefs.push_back({ move(pVar), obj });
						}
						else  param->to_variant(pvarg);
					}
					//TODO: add support for arrays and convert to safearray

					else  param->to_variant(pvarg);
					
				}
			}
		}
		// Initialize return variant.
		_variant_t pvRet;

		EXCEPINFO pexcepinfo;
		memset(&pexcepinfo, 0, sizeof pexcepinfo);
		UINT pnArgErr=0;

		// make the call 
		hr = comObj.pdispVal->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
			&dispparams, &pvRet,&pexcepinfo, &pnArgErr);


		switch (hr)
		{
		case S_OK:
			ret = jsrt::value::from_variant(&pvRet);

			for (auto item = byRefs.begin(); item != byRefs.end(); item++) {
				item->second.set_property(byRef, jsrt::value::from_variant(item->first.get()));
			}

			break;
		case DISP_E_EXCEPTION:
			jsrt::context::set_exception(jsrt::error::create((LPCWSTR)pexcepinfo.bstrDescription));
			break;
		case DISP_E_MEMBERNOTFOUND:
			jsrt::context::set_exception(jsrt::error::create(L"Object doesn't support method."));
			break;

		case DISP_E_TYPEMISMATCH:
		case DISP_E_OVERFLOW:
		case DISP_E_BADVARTYPE:
			jsrt::context::set_exception(jsrt::error::create_type_error(L"Parameter type mismatch."));
			break;
		case DISP_E_BADPARAMCOUNT:
		case DISP_E_PARAMNOTOPTIONAL:
			jsrt::context::set_exception(jsrt::error::create_type_error(L"Wrong number of parameters."));
			break;
		default:
			jsrt::context::set_exception(jsrt::error::create(L"Error invoking method."));
			break;
		}

		//cleanup
		if (dispparams.rgvarg != NULL && dispparams.cArgs != 0) {
			VARIANTARG FAR* pvarg = dispparams.rgvarg;
			UINT cArgs = dispparams.cArgs;
			while (cArgs--)
			{
				switch (pvarg->vt)
				{
				case VT_BSTR:
					VariantClear(pvarg);
					break;
				}
				++pvarg;
			}
			delete dispparams.rgvarg;
		}
	}
	return ret;

}


wstring RuntimeThread::cbGetFullPathName(wstring path) {
	return getFullPathName(path);
}

void RuntimeThread::cbCopyFile(std::wstring from, std::wstring to, jsrt::optional<bool> overwrite) {
	using namespace std::tr2::sys;
	if (from.empty() || to.empty()) {
		context.set_exception(jsrt::error::create(L"The filename, directory name, or volume label syntax is incorrect."));
		return;
	}

	from = getFullPathName(from);
	to = getFullPathName(to);
	if (to.back() == L'\\') to += getFilename(from);
	
	createDirectories(to);
	//copy_file(pFrom, pTo,
	//	overwrite.has_value() && overwrite.value() ? copy_option::overwrite_if_exists : copy_option::fail_if_exists);
	if (!CopyFile(from.c_str(), to.c_str(), overwrite.has_value() && overwrite.value() ? false : true)) {
		context.set_exception(jsrt::error::create(getLastErrorMsg()));
	}
}

bool RuntimeThread::cbMoveFile(std::wstring from, std::wstring to, jsrt::optional<bool> overwrite) {
	DWORD flag = MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH |
		((!overwrite.has_value() || overwrite.value()) ? MOVEFILE_REPLACE_EXISTING : 0x00000000);
	return MoveFileEx(from.c_str(), to.c_str(),flag)!=0;
}

jsrt::value RuntimeThread::cbGetFile(std::wstring path) {
	unique_ptr<file_object> obj = make_unique<file_object>(path);
	file_object *fObj = obj.get();
	fileObjects.push_back(move(obj));

	return fObj->createjsObject();
}