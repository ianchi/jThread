// jThread.exe
// JScript Runtime Host with Multithreading V1.0
//
// Author: Adrián Panella
// Description: enables running JScript files with multithreading support using the new Chakra engine (IE11)
 

#include "stdafx.h"


using namespace std;
#include "jThread.h"
#include "ExecProcess.h"
#include "winUtil.h"
#include "basic_process.h"
#include "RuntimeThread.h"

basic_process HostProcess;


//
// Method to start up debugging in the current context.
//

void startDebugger(IDebugApplication* &debugApplication, bool attach)
{
	HRESULT hr = S_OK;
	IClassFactory *classFactory = nullptr;
	IProcessDebugManager *pdm = nullptr;

	// Initialize COM because we're going to have to talk to some COM interfaces.
	if (CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED) != S_OK) goto error;

	// Get a pointer to the process debug manager (PDM).
	if (CoGetClassObject(__uuidof(ProcessDebugManager), CLSCTX_INPROC_SERVER, NULL, __uuidof(IClassFactory), (LPVOID *)&classFactory) != S_OK)
		goto error;
	if (classFactory->CreateInstance(0, _uuidof(IProcessDebugManager), (LPVOID *)&pdm) != S_OK)
		goto error;

	// Get the default application.
	if (pdm->GetDefaultApplication(&debugApplication) != S_OK)
		goto error;


	if (!debugApplication->FCanJitDebug()) goto error;
	if(!debugApplication->FIsAutoJitDebugEnabled()) goto error;

	if (attach) {
		hr=debugApplication->StartDebugSession();
		debugApplication->CauseBreak();
	}

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
		debugApplication = nullptr;
	}

	else fwprintf(stderr, L"debugger set.\n");
}


//main application entry point
int wmain(int argc, wchar_t* argv[])
{

	system("pause");

	//Set active Code Page to consistently show UNICODE output
	auto cpIN = GetConsoleCP();
	auto cpOUT = GetConsoleOutputCP();
	auto cp = GetACP();
	SetConsoleCP(cp);
	SetConsoleOutputCP(cp);

	HostInfo hostInfo;
	try {
		{
			wchar_t *pgm;
			_get_wpgmptr(&pgm); //gets the executable 
			tr2::sys::wpath pathHost = pgm;
			hostInfo.version = L"v0.1beta";
			hostInfo.name = pathHost.leaf();
			hostInfo.path = pathHost.parent_path();
		}
		ParseArguments args(L"PV?", { { L'T', L"[0-9]+" }, { L'D', L"(AUTO|BREAK)" } }, argc, argv);
		HostProcess.setOptions(args);

		if (!args.parseOK || args.boolOpts[L'?']) { //si hay errores en los argumentos o se indicó /? mostrar la ayuda
			wcout << getTextResource(L"USAGE");
			if (args.boolOpts[L'?']) wcout << getTextResource(L"HELP");
			//return args.parseOK ? 0 : 1;
		}
		else { //sino correr el script
			
			if (args.strOpts[L'D'] == L"AUTO") startDebugger(hostInfo.debugApplication, false);
			if (args.strOpts[L'D'] == L"BREAK") startDebugger(hostInfo.debugApplication, true);
			

			HostProcess.start(L" - jThread started running scripts ");

			auto rtm = static_cast<RuntimeThread*>(HostProcess.addChild(make_unique<RuntimeThread>(args, &hostInfo)));

			rtm->startThread();

			bool result=rtm->wait(args.strOpts.find(L'T') == args.strOpts.end() || stoul(args.strOpts[L'T']) == 0 ? INFINITE : stoul(args.strOpts[L'T'])*1000);


			rtm->flush(); //print buffered output (stdErr)

			HostProcess.end(wstring(L" - jThread ") + (result ? L"finished " : L"timeout ") + L"running " +
				to_wstring(rtm->descendants) + L" script" + (rtm->descendants>1 ? L"s" : L""));

			if (!result) HostProcess.error(L"Timeout Error after " + to_wstring(HostProcess.getDuration()) + L"milliseconds");

			//HostProcess.flush();
		}
		
	}
	catch (...) {
		wcerr << "jThread - Fatal error." << endl;
	}

	system("pause");
	//resets original code page
	SetConsoleCP(cpIN);
	SetConsoleOutputCP(cpOUT);
	return 0;
}




