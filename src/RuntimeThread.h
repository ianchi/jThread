#pragma once
#include "basic_process.h"
#include "ParseArguments.h"
#include "winUtil.h"
#include "jsrt-wrappers\jsrt-proxy_object.h"
#include "Profiler.h"
#include "file_object.h"

class RuntimeThread :
	public basic_process
{
public:

	std::timed_mutex runningMutex;
	std::future<void> futureReturn;

	jsrt::runtime runtime;
	jsrt::context context;
	std::wstring strScript;
	bool debugBreak;
	bool profile;

	std::unique_ptr<jsrt::proxy_object<RuntimeThread>> jThread;
	std::unique_ptr<jsrt::proxy_object<RuntimeThread>> proxyInParent;
	std::vector<std::unique_ptr<file_object>> fileObjects;

	HostInfo *hostInfo = nullptr;

	Profiler *profiler = nullptr;


public:
	RuntimeThread() {}
	RuntimeThread(ParseArguments args, HostInfo *hInfo) 	{
		setOptions(args);
		hostInfo = hInfo;

		if (!readFile(strPath + L"\\" + strName, strScript)) {
			raise(L"jThread - " + strScript);
			strScript = L"";
		}

		debugBreak = args.strOpts[L'D'] == L"BREAK" ? true : false;
		profile = args.boolOpts[L'P'];
	}

	RuntimeThread(RuntimeThread *pnt, const std::wstring &filename, const std::wstring *script = nullptr, const std::vector<std::wstring> *arguments = nullptr)
	{
		outputMode = pnt->outputMode | (DWORD)outputModeEnum::batchOut | (DWORD)outputModeEnum::batchErr;
		hostInfo = pnt->hostInfo;
		debugBreak = pnt->debugBreak;
		profile = pnt->profile;

		if (arguments != nullptr) strArguments = *arguments;

		//if no script text, asume read from file
		if (script == nullptr) {
			strName = getFilename(filename);
			strPath = getParentFolder(getFullPathName(filename));
			if (!readFile(strPath + L"\\" + strName, strScript)) {
				raise(L"jThread - " + strScript);
				strScript = L"";
			}

		}
		else{
			strName = filename;
			strScript = *script;
		}

	}

	
	~RuntimeThread();

	void startDebugging();
	void startProfiling();

	// raises an exception in the current script or writes to STDERR
	void raise(std::wstring msg) { 
		if (parent ==nullptr)
			error(msg);
		else
			jsrt::context::set_exception(jsrt::error::create(msg));
	};

	jsrt::value newThread(const std::wstring &filename, std::wstring *script, const std::vector<std::wstring> *arguments);
	jsrt::value newProcess(std::wstring command, std::wstring arguments);

	void startThread();
	void runThread();
	std::wstring runScript(const std::wstring &script, std::wstring name=L"");
	void createjThreadProxy();
	jsrt::value createChildProxy();

	//overloaded wait
	virtual bool wait(DWORD milliseconds = INFINITE){
		if (milliseconds == INFINITE || futureReturn.wait_for(std::chrono::milliseconds(milliseconds)) == std::future_status::ready)
		{
			futureReturn.get();
			return true;
		}
		else return false;
	}
		
	void RuntimeThread::terminate();


	//callbacks for jThread object

	std::wstring cbGetCurrentFolder() {return std::tr2::sys::current_path<std::wstring>(); }
	void cbSetCurrentFolder(std::wstring folder) {std::tr2::sys::current_path(std::tr2::sys::wpath(folder)); };
	std::wstring cbGetInput(jsrt::optional<std::wstring> msg) {
		std::wstring ret;
		if (outputMode & outputModeEnum::batchOut) 
			jsrt::context::set_exception(jsrt::error::create(L"Cannot use keyboard input in async mode."));
		else {
			if(msg.has_value()) std::wcout << msg.value();  
			std::wcin >> ret;
		}
		return ret;
	}
	void cbSleep(DWORD time) { std::this_thread::sleep_for(std::chrono::milliseconds(time)); }
	std::wstring cbGetUserName() { return getUser();}

	void cbLoadModule(std::wstring scriptPath);
	jsrt::value cbRunScriptAsync(std::wstring scriptFilename, const jsrt::rest<jsrt::value> arguments);
	jsrt::value RuntimeThread::cbEvalAsync(std::wstring script, const jsrt::rest<jsrt::value> arguments);
	jsrt::value RuntimeThread::cbFunctionAsync(jsrt::value func, const jsrt::rest<jsrt::value> arguments);

	//callbacks for async object
	bool RuntimeThread::cbWait(jsrt::optional<DWORD> milliseconds);
	jsrt::value cbGetReturnValue() {
		return returnValue.eof() ? jsrt::context::undefined() : RuntimeThread::jsonParse(returnValue.str());
	};

	//ActiveX support
	jsrt::value cbCreateObject(std::wstring progID) {
		return cbActiveXObject(jsrt::call_info(jsrt::value(), jsrt::value(), true), progID);
	}
	static jsrt::value cbActiveXObject(const jsrt::call_info &info, std::wstring progID);
	jsrt::value cbInvoke(jsrt::value obj, std::wstring method, jsrt::rest<jsrt::value> params);
	
	//file support
	void cbCopyFile(std::wstring from, std::wstring to, jsrt::optional<bool> overwrite);
	bool cbMoveFile(std::wstring from, std::wstring to, jsrt::optional<bool> overwrite);
	bool cbDeleteFile(std::wstring file) {
		return std::tr2::sys::remove_filename(std::tr2::sys::wpath(file));
	}
	std::wstring cbGetFullPathName(std::wstring path);
	jsrt::value cbGetFile(std::wstring path);

	//exposes JScript JSON functionality
	static std::wstring jsonStringify(jsrt::value param);
	static jsrt::value jsonParse(std::wstring param);


};

