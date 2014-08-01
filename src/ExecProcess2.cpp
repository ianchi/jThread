#include "stdafx.h"



using namespace std;

#include "ExecProcess2.h"

ExecProcess::ExecProcess(JSRuntimeInstance *parent) : parentRT(parent), state(instanceState::notRun), jsProxy(*this),
stdIn_Rd(nullptr), stdIn_Wr(nullptr), stdOut_Rd(nullptr), stdOut_Wr(nullptr), stdErr_Rd(nullptr), stdErr_Wr(nullptr)
{
	ZeroMemory(&procInfo, sizeof(procInfo));
}

void ExecProcess::create(wstring command, wstring args) {
	startTime = chrono::system_clock::now();
	state = instanceState::running;

	STARTUPINFO options;
	SECURITY_ATTRIBUTES saAttr;
	ZeroMemory(&options, sizeof(options));

	//formats executable and parameters
	if (expandEnvironmentStrings(command, strCommand) &&
		expandEnvironmentStrings(args, strArguments) &&
		expandPath(strCommand, strCommand)) {


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
			auto result = CreateProcess(strCommand.c_str(),   //  module name 
				(wchar_t *)strArguments.c_str(),        // Command line
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
				std::async(launch::async, ExecProcess::pipeListener, stdOut_Rd, &stdOut_buf);
				std::async(launch::async, ExecProcess::pipeListener, stdErr_Rd, &stdErr_buf);

				//launch thread to log finish time and clean-up
				std::async(launch::async, bind(&ExecProcess::finishListener, this));
				return;
			}
		}


	}
	//no se pudo lanzar
	endTime = chrono::system_clock::now();
	state = instanceState::idle;
}

void ExecProcess::flush() { //escribe el buffer en los stream de la consola
	if (parentRT->hostOpt.verbose) wcout << logTimeMessage(startTime, endTime, L" - Start executable " + strCommand, timeMessageMode::start) << endl;
	wcout << stdOut_buf.rdbuf();
	if (parentRT->hostOpt.verbose) wcout << logTimeMessage(startTime, endTime, L" - End executable " + strCommand, timeMessageMode::end) << endl;

	wcerr << stdErr_buf.rdbuf();
	if (parentRT->hostOpt.verbose && (int)stdErr_buf.tellp() != 0) wcerr << "End executable " << strCommand << " errors.\n";

}

void ExecProcess::finishListener() {
	if (procInfo.hProcess != nullptr) WaitForSingleObject(procInfo.hProcess, INFINITE);
	// cancels IO to unblock listener thread
	CancelIoEx(stdOut_Rd, nullptr);
	CancelIoEx(stdErr_Rd, nullptr);
	// close all handles
	CloseHandle(stdOut_Rd); stdOut_Rd = nullptr;
	CloseHandle(stdErr_Rd); stdErr_Rd = nullptr;

	GetExitCodeProcess(procInfo.hProcess, &exitCode);
	CloseHandle(procInfo.hProcess); procInfo.hProcess = nullptr;
	CloseHandle(procInfo.hThread); procInfo.hThread = nullptr;

	endTime = chrono::system_clock::now();
	state = instanceState::idle;
}
void ExecProcess::wait(const jsrt::call_info &info, jsrt::optional<wstring> str, jsrt::optional<wstring> str2) {
	if (procInfo.hProcess != nullptr) {
		WaitForSingleObject(procInfo.hProcess, INFINITE);
		return ;
	}
	else return ;
}



jsrt::object ExecProcess::createJsObject()
{
	jsProxy.bindMethod(L"wait", &ExecProcess::wait);


	return jsProxy;

	}