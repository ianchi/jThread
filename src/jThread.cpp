// jThread.exe
// JScript Runtime Host V1.0
//
// Autor: Adrián Panella
// Descripción: permite correr scripts .js usando el nuevo motor de IE11 

#include "stdafx.h"


using namespace std;
#include "jThread.h"
#include "ExecProcess2.h"

GlobalHostOptions global;

//punto de entrada de la aplicación
int wmain(int argc, wchar_t* argv[])
{
	
	

	try {
		chrono::time_point<chrono::system_clock> startTime, endTime;
		startTime = chrono::system_clock::now();
		{
			wchar_t *pgm;
			_get_wpgmptr(&pgm); //gets the executable 
			tr2::sys::wpath pathHost = pgm;
			global.version = L"v1.0";
			global.name = pathHost.leaf();
			global.path = pathHost.parent_path();
		}
		HostOptions hostOpts(argc, argv);

		if (!hostOpts.parseOK || hostOpts.help) { //si hay errores en los argumentos o se indicó /? mostrar la ayuda
			showHelp(hostOpts.help);
			wstring x;
			wcin >> x;
			return hostOpts.parseOK ? 0 : 1;
		}
		else { //sino correr el script
			if (hostOpts.verbose) wcout << logTimeMessage(startTime, endTime, L" - jThread started running scripts ", timeMessageMode::start) << endl;

			auto rtm = global.newInstance(hostOpts);
			rtm->run();
			//rtm->asyncRun();

			//espera que terminen de correr todas las instancias
			UINT i;
			for (i = 0; i < global.jsRTinstances.size(); i++) {
				if (global.jsRTinstances[i]->futureReturn.valid()) (global.jsRTinstances[i]->futureReturn.get());
				if (global.jsRTinstances[i]->runtime.is_valid()) global.jsRTinstances[i]->runtime.dispose();
				global.jsRTinstances[i]->flush();

			}
			endTime = chrono::system_clock::now();
			if (hostOpts.verbose) wcout << logTimeMessage(startTime, endTime, L" - jThread finished running " + to_wstring(i) + L" script" + (i>1 ? L"s" : L""), timeMessageMode::end) << endl;
		}
		
	}
	catch (...) {
		wcerr << "jThread - Fatal error." << endl;
	}

	wstring x;
	wcin >> x;
	return 0;
}


bool readFile(wstring filename, wstring &content) {
	wifstream file;
	wstringstream out;


	file.exceptions(ios::failbit); // errores como excepciones
	try {
		file.open(filename, ios_base::in, _SH_DENYWR);
		out << file.rdbuf();
		content = out.str();
		return true;
	}
	catch (ios_base::failure f) {
		content = L"Cannot read file " + filename;
	}


	return false;
}



/*
* callbacks que van a pasarse como Metodos
*
*/

// Callback to run an external command and run it. 
jsrt::object RunCommand(const jsrt::call_info &info, wstring command, wstring params=L"", bool wait=false, bool show=true) {

	wstring strCommand, strArgs;
	JSRuntimeInstance *self = JSRuntimeInstance::getSelfPointer(info);
	
	self->childProcess.push_back(make_unique<ExecProcess>(self));
	ExecProcess *newProcess = (self->childProcess.back()).get();

	newProcess->create(command, params);

	return newProcess->createJsObject();
}

void RunCommand2(const jsrt::call_info &info, wstring command, wstring args)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));


	if(! CreateProcess(command.c_str(),   //  module name 
		(wchar_t *)args.c_str(),        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		true,          // Set handle inheritance to FALSE
		CREATE_UNICODE_ENVIRONMENT ,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi))           // Pointer to PROCESS_INFORMATION structure
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return;
	}


	// Wait until child process exits.
	WaitForSingleObject(pi.hProcess, INFINITE);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	wchar_t str[MAX_PATH];

	SHELLEXECUTEINFO sei = { 0 };
	sei.lpFile = str;
	sei.cbSize = sizeof(SHELLEXECUTEINFO);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_DOENVSUBST | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
	sei.lpVerb = L"open";
	//sei.lpParameters = strArgs.c_str();
	//sei.nShow = show ? SW_SHOWNORMAL : SW_HIDE;

	sei.lpFile = str;

	auto result = ShellExecuteEx(&sei);

	CloseHandle(sei.hProcess);


}


// Callback to load a script and run it. jThread.runScript(scriptName, scriptParams...)
bool RunScript(const jsrt::call_info &info, wstring scriptFilename)
{
	

	if (scriptFilename.empty())
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"not enough arguments"));
		return false;
	}
	wstring script;
	if (!readFile(scriptFilename, script)) {
		jsrt::context::set_exception(jsrt::error::create(script));
		return false;
	}
	JSRuntimeInstance* self = JSRuntimeInstance::getSelfPointer(info);

	try {
		jsrt::context::run(script, 1, scriptFilename);
		return true;
	}
	catch (const jsrt::script_exception &e) { //runtime exception
		self->error(L"jThread - Unhandled exception in " + self->hostOpt.scriptName + L": " + e.what() + L"\n");
	}
	catch (const jsrt::script_compile_exception &e) { //error de compilación
		self->error(L"jThread - " + self->hostOpt.scriptName + L" " + e.what() + L"\n");
	}
	catch (const jsrt::exception &e) { //cualquier otra excepcion
		self->error(L"jThread - " + self->hostOpt.scriptName + L" " + e.what() + L"\n");
	}

	return false;
}

// Callback to load an async script

jsrt::value FunctionAsync(const jsrt::call_info &info, const vector<jsrt::value> &arguments)
	//jsrt::object jsFunction, jsrt::optional<jsrt::object> args, jsrt::optional<jsrt::object> opts)
{

	if (arguments.empty())
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"wrong number of arguments"));
		return jsrt::object();
	}

	//extracts code from function
	if (arguments[0].type() != JsValueType::JsFunction)
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"first argument must be a function"));
		return jsrt::object();
	}
	jsrt::object jsFunction(arguments[0]);
	jsrt::function<wstring> jsToString(jsFunction.get_property<jsrt::object>(jsrt::property_id::create(L"toString")));
	wstring script = jsToString(jsFunction);
	//extracts function name
	wsmatch	res;
	regex_search(script, res, wregex(L"^ *function *([^( ]*)", regex::ECMAScript));
	//if it is an anonymous function, asign it to be able to call it

	wstring scriptName = res[1];
	if (scriptName == L"") {
		script = L"var anonymousFunction= " + script;
		scriptName = L"anonymousFunction";
	}
	//genera una referencia al objeto runtime con la info del objeto js
	JSRuntimeInstance* self = JSRuntimeInstance::getSelfPointer(info);

	HostOptions newOptions(self->hostOpt, scriptName);

	wstring args,obj;
	for (UINT i = 1; i < arguments.size(); i++) {
		obj = JSRuntimeInstance::jsStringify(arguments[i]);
		newOptions.scriptArguments.push_back(obj);
		args += (args.empty() ? L"" : L", ") + obj;
	}

	script += L"\n var result= " + scriptName + L"(" +args + L");\nresult;";


	//setea las nuevas opciones
	//serializa el objeto para pasarlo como parametro a otra instacia (no se pueden compartir objetos)
	newOptions.script = script;

	//agrega la instancia y la lanza
	auto newRT = global.newInstance(newOptions, scriptMode::async | scriptMode::eval);

	auto retObj = newRT->createAsyncObject();

	newRT->asyncRun();

	return retObj;

}

// Callback to load an async script
jsrt::value EvalAsync(const jsrt::call_info &info, const vector<jsrt::value> &arguments)
{
	if (arguments.empty())
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"wrong number of arguments"));
		return jsrt::object();
	}
	wstring script = jsrt::string(arguments[0]).data();
	if (script.empty())
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"not enough arguments"));
		return jsrt::object();
	}

	//genera una referencia al objeto runtime con la info del objeto js
	JSRuntimeInstance* self = JSRuntimeInstance::getSelfPointer(info);

	//setea las nuevas opciones
	//serializa el objeto para pasarlo como parametro a otra instacia (no se pueden compartir objetos)
	HostOptions newOptions(self->hostOpt, L"\\Eval code");
	newOptions.script = script;

	wstring obj;
	for (UINT i = 1; i < arguments.size(); i++) {
		obj = JSRuntimeInstance::jsStringify(arguments[i]);
		newOptions.scriptArguments.push_back(obj);
	}


	//agrega la instancia y la lanza
	auto newRT = global.newInstance(newOptions, scriptMode::async | scriptMode::eval);

	auto retObj = newRT->createAsyncObject();

	newRT->asyncRun();

	return retObj;

}
// Callback to load an async script
jsrt::value RunScriptAsync(const jsrt::call_info &info, const vector<jsrt::value> &arguments)
{
	if (arguments.empty())
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"wrong number of arguments"));
		return jsrt::object();
	}

	wstring scriptFilename = jsrt::string(arguments[0]).data();
	if (scriptFilename.empty())
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"not enough arguments"));
		return jsrt::object();
	}
	wstring script;
	if (!readFile(scriptFilename, script)) {
		jsrt::context::set_exception(jsrt::error::create(script));
		return jsrt::object();
	}

	//genera una referencia al objeto runtime con la info del objeto js
	JSRuntimeInstance* self = JSRuntimeInstance::getSelfPointer(info);

	//setea las nuevas opciones
	//serializa el objeto para pasarlo como parametro a otra instacia (no se pueden compartir objetos)
	HostOptions newOptions(self->hostOpt, scriptFilename);
	wstring obj;
	for (UINT i = 1; i < arguments.size(); i++) {
		obj = JSRuntimeInstance::jsStringify(arguments[i]);
		newOptions.scriptArguments.push_back(obj);
	}

	//agrega la instancia y la lanza
	auto newRT = global.newInstance(newOptions, scriptMode::async);

	auto retObj = newRT->createAsyncObject();

	newRT->asyncRun();

	return retObj;
}


wstring JSRuntimeInstance::jsStringify(jsrt::value param) {
	jsrt::object JSON = jsrt::context::global().get_property<jsrt::object>(jsrt::property_id::create(L"JSON"));
	jsrt::function<wstring, jsrt::value> stringify(JSON.get_property<jsrt::object>(jsrt::property_id::create(L"stringify")));
	return stringify(JSON, param);
}
jsrt::value JSRuntimeInstance::jsParse(wstring param) {
	jsrt::object JSON = jsrt::context::global().get_property <jsrt::object>(jsrt::property_id::create(L"JSON"));
	jsrt::function<jsrt::value, wstring> parse(JSON.get_property <jsrt::value>(jsrt::property_id::create(L"parse")));
	return (param == L"undefined") ? jsrt::context::undefined() : parse(JSON, param);
}

JSRuntimeInstance* JSRuntimeInstance::getSelfPointer(const jsrt::call_info &info)  {
	return static_cast<JSRuntimeInstance*>(jsrt::external_object(info.this_value()).data());
}



jsrt::object JSRuntimeInstance::createAsyncObject()
{
	//genera el objeto a exponer al script y guarda una referencia a la instancia actual 
	jsrt::external_object asyncObject = jsrt::external_object::create(this, nullptr);

	createGetSetProp<wstring>(asyncObject, L"stdOut",
		[](const jsrt::call_info &info)->wstring {GET_SELF; return self->stdOut->str(); }, nullptr);
	createGetSetProp<wstring>(asyncObject, L"stdErr",
		[](const jsrt::call_info &info)->wstring {GET_SELF; return self->stdErr->str(); }, nullptr);


	createGetSetProp<bool>(asyncObject, L"isRunning",
		[](const jsrt::call_info &info)->bool {GET_SELF; return (self->state == instanceState::running); }, nullptr);
	createGetSetProp<double>(asyncObject, L"runTime",
		[](const jsrt::call_info &info)->double {GET_SELF; 
		if (self->state == instanceState::notRun) return -1;
		else return ((double)(chrono::duration_cast<chrono::milliseconds>(((self->state == instanceState::running) ? chrono::system_clock::now() : self->endTime) - self->startTime)).count()) / 1000; }, nullptr);
	createGetSetProp<jsrt::value>(asyncObject, L"returnValue",
		[](const jsrt::call_info &info)->jsrt::value {GET_SELF; 
		if (self->state == instanceState::notRun || self->state == instanceState::running) return jsrt::context::undefined();
		else return jsParse(self->scriptResult); }, nullptr);
	createFunctionProp<void>(asyncObject, L"wait",
		[](const jsrt::call_info &info)->void {GET_SELF; 
		while (self->state == instanceState::notRun) this_thread::sleep_for(chrono::milliseconds(5));
		lock_guard<mutex> lockM(self->runningMutex); 
	});



	return asyncObject;
}

// Crea el objeto "jThread" y las propiedades que se van a exponer como interfase con el Script.
void JSRuntimeInstance::createjThreadObject()
{

	try {
		jsrt::object globalObject = context.global();

		//genera el objeto jThread a exponer al script y guarda una referencia a la instancia actual 
		jsrt::external_object hostObject = jsrt::external_object::create(this, nullptr);
		createValueProp<jsrt::external_object>(globalObject, L"jThread", hostObject);

		//agrega las propiedades al hostObject

		//read-only properties
		createValueProp<wstring>(hostObject, L"version", global.version);
		createValueProp<wstring>(hostObject, L"name", global.name);
		createValueProp<wstring>(hostObject, L"path", global.path);
		createValueProp<wstring>(hostObject, L"scriptName", hostOpt.scriptName);
		createValueProp<wstring>(hostObject, L"scriptPath", hostOpt.scriptPath);
		createValueProp<wstring>(hostObject, L"scriptFullName", hostOpt.scriptPath + L"\\" + hostOpt.scriptName);
		createValueProp<int>(hostObject, L"isAsync", (mode == scriptMode::async));

		// crea el array con los argumentos del script
		jsrt::array<jsrt::value> argumentsArray = jsrt::array<jsrt::value>::create(hostOpt.scriptArguments.size());
		for (UINT i = 0; i < hostOpt.scriptArguments.size(); i++) 
			argumentsArray[i] = mode & scriptMode::async ?
					jsParse(hostOpt.scriptArguments[i]) : //if called async they are JSON encoded
					(jsrt::value)jsrt::string::create(hostOpt.scriptArguments[i]); //from command line they are strings
			
		createValueProp<jsrt::array<jsrt::value>>(hostObject, L"arguments", argumentsArray);

		createGetSetProp<wstring>(hostObject, L"stdOut",
			[](const jsrt::call_info &info)->wstring {GET_SELF; return self->stdOut->str(); }, nullptr);
		createGetSetProp<wstring>(hostObject, L"stdErr",
			[](const jsrt::call_info &info)->wstring {GET_SELF; return self->stdErr->str(); }, nullptr);

		//read-write properties
		createGetSetProp<wstring>(hostObject, L"currentFolder",
			[](const jsrt::call_info &info)-> wstring {return tr2::sys::current_path<wstring>(); },
			[](const jsrt::call_info &info, wstring param) ->void {tr2::sys::current_path(tr2::sys::wpath(param)); });


		//methods
		createFunctionProp<void, wstring>(hostObject, L"echo",
			[](const jsrt::call_info &info, wstring p1)-> void {GET_SELF; self->echo(p1 + L"\n"); });
		createFunctionProp<void, wstring>(hostObject, L"error",
			[](const jsrt::call_info &info, wstring p1)-> void {GET_SELF; self->error(p1 + L"\n"); });
		createFunctionProp<wstring, wstring>(hostObject, L"input",
			[](const jsrt::call_info &info, wstring p1)-> wstring {GET_SELF;
				wstring ret;
				if (self->mode != scriptMode::async) { wcout << p1;  wcin >> ret; }
				else jsrt::context::set_exception(jsrt::error::create(L"Cannot use keyboard input in async mode."));
				return ret; });

		createFunctionProp<void, double>(hostObject, L"sleep",
			[](const jsrt::call_info &info, double p1)-> void { this_thread::sleep_for(chrono::milliseconds((int)p1)); });
		createFunctionProp<wstring>(hostObject, L"getUserName",
			[](const jsrt::call_info &info)-> wstring {	wstring name; DWORD size=0;
			GetUserName(&name[0], &size); name.resize(size);
			GetUserName(&name[0], &size); return name;});


		createFunctionProp<bool, wstring>(hostObject, L"runScript", RunScript);
		createFunctionProp(hostObject, L"runScriptAsync", RunScriptAsync);
		createFunctionProp(hostObject, L"evalAsync", EvalAsync);
		createFunctionProp(hostObject, L"functionAsync", FunctionAsync);
		createFunctionProp(hostObject, L"exec", RunCommand);




		//cierra el objeto
		hostObject.prevent_extension();

	}
	catch (const jsrt::script_exception &e) { //runtime exception
		raise(L"jThread - Unhandled exception in " + hostOpt.scriptName + L": " + e.what() + L"\n");
	}
}


wstring logTimeMessage(chrono::time_point<chrono::system_clock> &start, chrono::time_point<chrono::system_clock> &end, wstring msg, timeMessageMode mode) {

	time_t	loctime = chrono::system_clock::to_time_t(mode == timeMessageMode::start ? start :  end);
	tm loctime_tm;
	wostringstream out;

	localtime_s(&loctime_tm, &loctime);

	if (mode !=timeMessageMode::dura)
		out << put_time(&loctime_tm, L"%d-%b-%y %X") << msg;
	if (mode == timeMessageMode::dura || mode == timeMessageMode::end)
		out << L" (" << setprecision(1) << (chrono::duration_cast<chrono::milliseconds>(end - start).count()) / 1000 << L"segs)";
	
	return out.str();
}

void JSRuntimeInstance::run()
{ 
	lock_guard<mutex> lockM(runningMutex); //blocks until end of function. 

	state = instanceState::running;
	startTime = chrono::system_clock::now();

	try
	{
		wstring script;
		if (!runtime.is_valid())
			runtime = jsrt::runtime::create(JsRuntimeAttributeNone, JsRuntimeVersionEdge);
		if (!context.is_valid())
			context = runtime.create_context();
		//no se puede correr dos veces el mismo runtime

		if (hostOpt.verbose && !(mode & scriptMode::async)) wcout << logTimeMessage(startTime, endTime, L" - Start script " + hostOpt.scriptName, timeMessageMode::start) << endl;
		
		if (mode & scriptMode::eval) { script = move(hostOpt.script); }
		else if(!readFile(hostOpt.scriptPath + L"\\" + hostOpt.scriptName, script)) {
			raise(L"jThread - " + script);
			script = L"";
		}

		if(!script.empty()) {
			jsrt::context::scope scope(context);
			jsrt::value result;
			createjThreadObject();

			try {
				result=context.evaluate(script, 0, hostOpt.scriptName);
				scriptResult=jsStringify(result);
			}
			catch (const jsrt::script_exception &e) { //runtime exception
				error(L"jThread - Unhandled exception in " + hostOpt.scriptName + L": " + e.what() + L"\n");
			}
			catch (const jsrt::script_compile_exception &e) { //error de compilación
				error(L"jThread - " + hostOpt.scriptName + L" " + e.what() + L"\n");
			}
			catch (const jsrt::exception &e) { //cualquier otra excepcion
				error(L"jThread - " + hostOpt.scriptName + L" " + e.what() + L"\n");
			}


		}
	}
	catch (...){
		raise(L"jThread - Fatal error.\n");
	}
	
	endTime = chrono::system_clock::now();
	state = instanceState::idle;
	if (hostOpt.verbose && !(mode & scriptMode::async)) wcout << logTimeMessage(startTime, endTime, L" - End script " + hostOpt.scriptName, timeMessageMode::end) << endl;
}


//reads text resource content
void textResource(wchar_t *res) {
	HRSRC hRes = FindResource(NULL, res, L"TEXTFILE");
	HGLOBAL hGlobal = NULL;

	if (hRes != NULL)
		hGlobal = LoadResource(NULL, hRes);

	if (hGlobal != NULL) {
		DWORD size = SizeofResource(NULL, hRes);
		const wchar_t *data = (const wchar_t *)LockResource(hGlobal); //it must be in UTF16 LE
		wcout.write(data+1, (size/2-1)); //skip first dword - file UTF16 byte order mark
	}

}


//shows Usage and extended Help
void showHelp(bool extendedHelp) {

	textResource(L"USAGE");
	if (extendedHelp) textResource(L"HELP");
}



