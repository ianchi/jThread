#include "stdafx.h"



using namespace std;

#include "ExecProcess.h"
#include "winUtil.h"



void ExecProcess::startThread() {
	state = (DWORD)basic_process::stateEnum::running;
	start(L" - Start external process  " + strName);

	STARTUPINFO options;
	SECURITY_ATTRIBUTES saAttr;
	ZeroMemory(&options, sizeof(options));

	if (!strName.empty()) {


		// Set the bInheritHandle flag so pipe handles are inherited. 
		saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
		saAttr.bInheritHandle = TRUE;
		saAttr.lpSecurityDescriptor = NULL;

		// Create a pipe for the child process's STDOUT. Ensure the read handle to the pipe for STDOUT is not inherited 
		if (!(CreatePipe(&stdOut_Rd, &stdOut_Wr, &saAttr, 0) && SetHandleInformation(stdOut_Rd, HANDLE_FLAG_INHERIT, 0))) {
			if (stdOut_Rd != nullptr) CloseHandle(stdOut_Rd);
			if (stdOut_Wr != nullptr) CloseHandle(stdOut_Wr);
		}
		// Create a pipe for the child process's STDERR. Ensure the read handle to the pipe for STDERR is not inherited 
		else if (!(CreatePipe(&stdErr_Rd, &stdErr_Wr, &saAttr, 0) && SetHandleInformation(stdErr_Rd, HANDLE_FLAG_INHERIT, 0))) {
			if (stdErr_Rd != nullptr) CloseHandle(stdErr_Rd);
			if (stdErr_Wr != nullptr) CloseHandle(stdErr_Wr);
		}

		// Create a pipe for the child process's STDIN. Ensure the write handle to the pipe for STDIN is not inherited.
		else if (!(CreatePipe(&stdIn_Rd, &stdIn_Wr, &saAttr, 0) && SetHandleInformation(stdIn_Wr, HANDLE_FLAG_INHERIT, 0))) {
			if (stdIn_Rd != nullptr) CloseHandle(stdIn_Rd);
			if (stdIn_Wr != nullptr) CloseHandle(stdIn_Wr);
		}
		else {

			options.cb = sizeof(options);
			options.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			options.wShowWindow = SW_HIDE;
			options.hStdInput = stdIn_Rd;
			options.hStdOutput = stdOut_Wr;
			options.hStdError = stdErr_Wr;
			wstring command = strPath.empty() ? strName : strPath + L"\\" + strName;
			auto result = CreateProcess(command.c_str(),   //  module name 
				(wchar_t *)strArguments[0].c_str(),        // Command line
				NULL,           // Process handle not inheritable
				NULL,           // Thread handle not inheritable
				true,          // Set handle inheritance to FALSE
				CREATE_UNICODE_ENVIRONMENT,              // No creation flags
				NULL,           // Use parent's environment block
				NULL,           // Use parent's starting directory 
				&options,            // Pointer to STARTUPINFO structure
				&procInfo);           // Pointer to PROCESS_INFORMATION structure

			if (result) {

				//launch listener threads
				std::async(launch::async, ExecProcess::pipeListener, stdOut_Rd, &stdOutBuf);
				std::async(launch::async, ExecProcess::pipeListener, stdErr_Rd, &stdErrBuf);

				//launch thread to log finish time and clean-up
				std::async(launch::async, bind(&ExecProcess::finishListener, this));
				return;
			}
		}
	}
	//no se pudo lanzar
	state = (WORD)basic_process::stateEnum::terminated;
	end(L" - Abnormal process termination " + strName);
}


void ExecProcess::finishListener() {
	DWORD exitCode;
	if (procInfo.hProcess != nullptr) WaitForSingleObject(procInfo.hProcess, INFINITE);
	// cancels IO to unblock listener thread
	CancelIoEx(stdOut_Rd, nullptr);
	CancelIoEx(stdErr_Rd, nullptr);
	// close all handles
	CloseHandle(stdOut_Rd); stdOut_Rd = nullptr;
	CloseHandle(stdErr_Rd); stdErr_Rd = nullptr;

	GetExitCodeProcess(procInfo.hProcess, &exitCode);
	returnValue.clear();
	returnValue << exitCode;
	CloseHandle(procInfo.hProcess); procInfo.hProcess = nullptr;
	CloseHandle(procInfo.hThread); procInfo.hThread = nullptr;

	state = (DWORD)basic_process::stateEnum::ended;
	end(L" - End process " + strName);

}

void ExecProcess::pipeListener(HANDLE file, wstringstream *buffer) {
	DWORD bytesRead, lastError;
	char aux[500];
	wstring waux;

	while (true) {
		if (ReadFile(file, aux, 500, &bytesRead, NULL)) {
			ACP_decode(aux, bytesRead,waux);
			*buffer << waux;
		}
		else
		{
			lastError = GetLastError();
			break;
		}
	}
}


bool ExecProcess::wait( jsrt::optional<double> milliseconds) {

	if (procInfo.hProcess != nullptr) {
			auto result=WaitForSingleObject(procInfo.hProcess, milliseconds.has_value() ? (DWORD)milliseconds.value() : INFINITE);
			return result == WAIT_OBJECT_0;
	}
	else return false ;
}


jsrt::value ExecProcess::createChildProxy()
{
	jsProxy.bindMethod(L"wait", &ExecProcess::wait);
	jsProxy.bindMethod(L"terminate", &ExecProcess::terminate);

	jsProxy.bindProperty(L"state", (DWORD *)&state, false);
	jsProxy.bindAccesor<double>(L"duration", &ExecProcess::getDuration, nullptr);
	jsProxy.bindProperty(L"processID", (DWORD *)&procInfo.dwProcessId, false);

	jsProxy.bindAccesor<wstring>(L"stdIn", nullptr, &ExecProcess::stdIn);
	jsProxy.bindAccesor<wstring>(L"stdOut", &ExecProcess::stdOut, nullptr);
	jsProxy.bindAccesor<wstring>(L"stdErr", &ExecProcess::stdErr, nullptr);
	jsProxy.bindAccesor<wstring>(L"returnValue", &ExecProcess::cbGetReturnValue, nullptr);

	return jsProxy;

	}