#pragma once

#include "jsrt-wrappers\jsrt-proxy_object.h"
#include "jThread.h"
#include "Header.h"
#include "winUtil.h"
#include "basic_process.h"

class ExecProcess : public basic_process {

private:
	PROCESS_INFORMATION procInfo;
	HANDLE stdIn_Rd, stdIn_Wr;
	HANDLE stdOut_Rd, stdOut_Wr;
	HANDLE stdErr_Rd, stdErr_Wr;

	jsrt::proxy_object<ExecProcess> jsProxy;

	void finishListener();
	static void pipeListener(HANDLE file, std::wstringstream *buffer);

	//MyObject comObj;

public:
	ExecProcess::ExecProcess(basic_process *pnt, const std::wstring &command, const std::wstring &args) : jsProxy(*this),
		stdIn_Rd(nullptr), stdIn_Wr(nullptr), stdOut_Rd(nullptr), stdOut_Wr(nullptr), stdErr_Rd(nullptr), stdErr_Wr(nullptr)	
	{
		ZeroMemory(&procInfo, sizeof(procInfo));

		outputMode = pnt->outputMode | (DWORD)outputModeEnum::batchOut | (DWORD)outputModeEnum::batchErr;

		//formats executable and parameters
		strArguments.push_back(L"");
		std::wstring auxCommand;
		expandEnvironmentStrings(command, auxCommand);
		expandEnvironmentStrings(args, strArguments[0]);

		std::tr2::sys::wpath aux = auxCommand;
		strName = aux.leaf();
		strPath = std::tr2::sys::complete(aux).parent_path();
		
	}
	~ExecProcess() {}

	void startThread();


	bool wait(jsrt::optional<double> milliseconds);

	void terminate() { 
		terminateProcessByHandle(procInfo.hProcess, INFINITE);
		end(L" - Abnormal process termination");
		state = (DWORD)stateEnum::terminated;
	};

	void stdIn(std::wstring val) { 
		DWORD written = 0;
		WriteFile(stdIn_Wr, val.c_str(), val.length()*2, &written, NULL);
	}

	std::wstring cbGetReturnValue() {
		return returnValue.str();
	};


	jsrt::value createChildProxy();

};

