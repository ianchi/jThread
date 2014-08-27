#include "stdafx.h"
#include "basic_process.h"

using namespace std;

basic_process::basic_process()  : state((DWORD)stateEnum::starting),
parent(nullptr), isExternal(false), outputMode(0), descendants(0)
{
	startTime = endTime = chrono::system_clock::now();
	outputMode = outputModeEnum::batchOut & outputModeEnum::batchErr & outputModeEnum::verbose;
}

basic_process::~basic_process()
{
}

void basic_process::setOptions(ParseArguments &args)
{
	strName = args.name;
	strPath = args.path;
	strArguments = args.params;

	outputMode = outputModeEnum::batchErr | (args.boolOpts[L'V'] ? (DWORD)outputModeEnum::verbose : (DWORD)0);


}
void basic_process::start(wstring msg, bool reset) {
	if (reset) {
		startTime = std::chrono::system_clock::now();
		state = (DWORD)0 & stateEnum::starting;
	}
	if (outputMode & outputModeEnum::verbose) logTimeMessage(msg, true);

}

void basic_process::end(std::wstring msg)  {
	endTime = std::chrono::system_clock::now();
	if (outputMode & outputModeEnum::verbose) logTimeMessage(msg, false);
}

void basic_process::logTimeMessage(std::wstring msg, bool isStart) {

	time_t	loctime = chrono::system_clock::to_time_t(isStart ? startTime : endTime);
	tm loctime_tm;
	wostringstream out;

	localtime_s(&loctime_tm, &loctime);


	out << (!isStart ? L"====== " : L"------ ") << put_time(&loctime_tm, L"%d-%b-%y %X") << msg << L" ";

	if (!isStart)
		out << L"(" << fixed << setprecision(1) << getDuration() / 1000 << L"s) ";
	
	wstring str = out.str();
	if (str.size() < 80) str.append(80 - str.size(), (!isStart ? L'=' : L'-'));
	if (isStart) str = L"\n" + str;

	echo(str);
}

void basic_process::error(wstring msg) { //escribe en el stream de error o en el buffer, según corresponda
	if (outputMode & outputModeEnum::none) return;
	stdErrBuf << msg << endl;
	if (!(outputMode & outputModeEnum::batchErr) )
		wcerr << msg << endl;
};

void basic_process::echo(wstring msg) { //escribe en el stream de error o en el buffer, según corresponda
	if (outputMode & outputModeEnum::none) return;
	stdOutBuf << msg << endl;
	if (!(outputMode & outputModeEnum::batchOut))
		wcout << msg << endl;
}

//
void basic_process::flush() { 
	if (outputMode & outputModeEnum::none) return;
	for (auto child = children.begin(); child != children.end(); child++)
		(*child)->flush();
	echo(L"flushing " + strName);
	if (outputMode & outputModeEnum::batchOut) {
		if (parent != nullptr) 
			parent->echo(stdOutBuf.str());
		else 
			wcout << stdOutBuf.rdbuf();
	}
	if (outputMode & outputModeEnum::batchErr) {
		if (!stdErrBuf.eof() && outputMode & outputModeEnum::verbose) 
			error(L"End errors transcript.");
		if (parent != nullptr) 
			parent->error(stdErrBuf.str());
		else 
			wcerr << stdErrBuf.rdbuf();
	}
	outputMode |= (DWORD)outputModeEnum::none; //no more output for the current process

}