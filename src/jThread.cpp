// jThread.exe
// JScript Runtime Host V1.0
//
// Autor: Adrián Panella
// Descripción: permite correr scripts .js usando el nuevo motor de IE11 

#include "stdafx.h"

using namespace std;

#include "jThread.h"

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
		wcout << global.name << L" | " << global.path << endl;
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
jsrt::object FunctionAsync(const jsrt::call_info &info, jsrt::object jsFunction, jsrt::optional<jsrt::object> args, jsrt::optional<jsrt::object> opts)
{
	//extracts code from function
	if (jsFunction.type() != JsValueType::JsFunction)
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"first argument must be a function"));
		return jsrt::object();
	}
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
	script += L"\n return " + scriptName + L"(" + JSRuntimeInstance::jsStringify(args.value()) + L")\n";

	//genera una referencia al objeto runtime con la info del objeto js
	JSRuntimeInstance* self = JSRuntimeInstance::getSelfPointer(info);

	//setea las nuevas opciones
	//serializa el objeto para pasarlo como parametro a otra instacia (no se pueden compartir objetos)
	HostOptions newOptions(self->hostOpt,scriptName, args.has_value() ? JSRuntimeInstance::jsStringify(args.value()) : L"", opts);
	newOptions.script = script;

	//agrega la instancia y la lanza
	auto newRT = global.newInstance(newOptions, scriptMode::async | scriptMode::eval);

	auto retObj = newRT->createAsyncObject();

	newRT->asyncRun();

	return retObj;

}

// Callback to load an async script
jsrt::object EvalAsync(const jsrt::call_info &info, wstring script, jsrt::optional<jsrt::object> args, jsrt::optional<jsrt::object> opts)
{

	if (script.empty())
	{
		jsrt::context::set_exception(jsrt::error::create_type_error(L"not enough arguments"));
		return jsrt::object();
	}

	//genera una referencia al objeto runtime con la info del objeto js
	JSRuntimeInstance* self = JSRuntimeInstance::getSelfPointer(info);

	//setea las nuevas opciones
	//serializa el objeto para pasarlo como parametro a otra instacia (no se pueden compartir objetos)
	HostOptions newOptions(self->hostOpt, L"\\Eval code", args.has_value() ? JSRuntimeInstance::jsStringify(args.value()) : L"", opts);
	newOptions.script = script;

	//agrega la instancia y la lanza
	auto newRT = global.newInstance(newOptions, scriptMode::async | scriptMode::eval);

	auto retObj = newRT->createAsyncObject();

	newRT->asyncRun();

	return retObj;

}
// Callback to load an async script
jsrt::object RunScriptAsync(const jsrt::call_info &info, wstring scriptFilename, jsrt::optional<jsrt::object> args, jsrt::optional<jsrt::object> opts)
{

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
	HostOptions newOptions(self->hostOpt, scriptFilename, args.has_value() ? JSRuntimeInstance::jsStringify(args.value()) : L"", opts);

	//agrega la instancia y la lanza
	auto newRT = global.newInstance(newOptions, scriptMode::async);

	auto retObj = newRT->createAsyncObject();

	newRT->asyncRun();

	return retObj;
}


wstring JSRuntimeInstance::jsStringify(jsrt::object param) {
	jsrt::object JSON = jsrt::context::global().get_property<jsrt::object>(jsrt::property_id::create(L"JSON"));
	jsrt::function<wstring, jsrt::object> stringify(JSON.get_property<jsrt::object>(jsrt::property_id::create(L"stringify")));
	return stringify(JSON, param);
}
jsrt::object JSRuntimeInstance::jsParse(wstring param) {
	jsrt::object JSON = jsrt::context::global().get_property <jsrt::object>(jsrt::property_id::create(L"JSON"));
	jsrt::function<jsrt::object, wstring> parse(JSON.get_property <jsrt::object>(jsrt::property_id::create(L"parse")));
	return parse(JSON, param);
}

JSRuntimeInstance* JSRuntimeInstance::getSelfPointer(const jsrt::call_info &info)  {
	return static_cast<JSRuntimeInstance*>(jsrt::external_object(info.this_value()).data());
}



jsrt::object JSRuntimeInstance::createAsyncObject()
{
	//genera el objeto a exponer al script y guarda una referencia a la instancia actual 
	jsrt::external_object asyncObject = jsrt::external_object::create(this, nullptr);

	createGetSetProp<bool>(asyncObject, L"isRunning",
		[](const jsrt::call_info &info)->bool {GET_SELF; return self->isRunning; }, nullptr);
	createGetSetProp<double>(asyncObject, L"runTime",
		[](const jsrt::call_info &info)->double {GET_SELF; 
	return ((double)(chrono::duration_cast<chrono::milliseconds>((self->isRunning ? chrono::system_clock::now() : self->endTime) - self->startTime)).count()) / 1000; }, nullptr);
	
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

		if (mode == scriptMode::async) {
			//de-serializa el objeto recibido como parametro utilizando la función nativa JSON.stringify(obj)
			jsrt::object argument = jsParse(hostOpt.scriptArguments[0]);
			createValueProp<jsrt::object>(hostObject, L"arguments", argument);
		}
		else {
			jsrt::array<std::wstring> argumentsArray = jsrt::array<std::wstring>::create(hostOpt.scriptArguments.size());
			for (UINT i = 0; i < hostOpt.scriptArguments.size(); i++)
				argumentsArray[i] = hostOpt.scriptArguments[i];
			createValueProp<jsrt::array<wstring>>(hostObject, L"arguments", argumentsArray);
		}


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

		createFunctionProp<void, jsrt::object>(hostObject, L"quit",
			[](const jsrt::call_info &info, jsrt::object param)-> void {GET_SELF; self->echo(L"quit not implemented yet.");});

		createFunctionProp<bool, wstring>(hostObject, L"runScript", RunScript);
		createFunctionProp<jsrt::object, wstring, jsrt::optional<jsrt::object>, jsrt::optional<jsrt::object>>(hostObject, L"runScriptAsync", RunScriptAsync);
		createFunctionProp<jsrt::object, wstring, jsrt::optional<jsrt::object>, jsrt::optional<jsrt::object>>(hostObject, L"evalAsync", EvalAsync);
		createFunctionProp<jsrt::object, jsrt::object, jsrt::optional<jsrt::object>, jsrt::optional<jsrt::object>>(hostObject, L"functionAsync", FunctionAsync);

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

	if (isRunning) return;
	else isRunning = true;
	startTime = chrono::system_clock::now();

	try
	{
		wstring script;
		if (!runtime.is_valid())
			runtime = jsrt::runtime::create(JsRuntimeAttributeNone, JsRuntimeVersionEdge);
		if (!context.is_valid())
			context = runtime.create_context();
		//no se puede correr dos veces el mismo runtime

		if (hostOpt.verbose) echo(logTimeMessage(startTime, endTime, L" - Start script " + hostOpt.scriptName, timeMessageMode::start) + L"\n");
		
		if (mode & scriptMode::eval) { script = move(hostOpt.script); }
		else if(!readFile(hostOpt.scriptPath + L"\\" + hostOpt.scriptName, script)) {
			raise(L"jThread - " + script);
			script = L"";
		}

		if(!script.empty()) {
			jsrt::context::scope scope(context);

			createjThreadObject();

			jsrt::value value; 
			JsValueRef strValue,stack = nullptr;
			auto ret = JsRunScript(L"\"hola \";", 1, L"", &stack);
			JsValueType tipo; JsErrorCode errorC;
			JsGetValueType(stack, &tipo);
			if (tipo != JsString) {
				errorC = JsConvertValueToString(stack, &strValue);
			}
			else strValue = stack;
				const wchar_t *resultptr = nullptr;
				size_t length;

				errorC = JsStringToPointer(strValue, &resultptr, &length);
				wstring result;
				if (errorC == JsNoError)
				{
					result = std::wstring(resultptr, length);
				}



			try {
				jsrt::runtime::translate_error_code(ret);
				context.run(script, 0, hostOpt.scriptName);
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
	isRunning = false;
	if (hostOpt.verbose) echo(logTimeMessage(startTime, endTime, L" - Start script " + hostOpt.scriptName, timeMessageMode::end) + L"\n");
}



//Muestra la ayuda de la aplicación
void showHelp(bool extendedHelp) {
	wcout << L"\n"
		L"jThread - JavaScript Runtime Host with Multithreading " << global.version <<
		L"\nBy Adrian Panella\n\n" <<
		L"Usage: jThread [host options] ScriptName.extension [script arguments]\n\n" <<
		L"Host Options:\n" <<
		L"   /?             Show extended help.\n" <<
		L"   /D             Enable script debugging.\n" <<
		L"   /P[:log file]  Activate profiling. If no log file supplied uses console output.\n" <<
		L"   /O:file        Redirects output to \"file\" instead of console.\n" <<
		L"   /E:file        Redirects errors to \"file\" instead of console.\n" <<
		L"   /T:minutes     Defines the maximum amount of minutes that the script can run before it is terminated.\n" <<
		L"                  Defaults to 0 (no time limit).\n" <<
		L"   /V             Verbose. Outputs log info on script start and finish.\n\n" <<
		L"The application generates output in UNICODE. " <<
		L"To correctly see special characters in the console output, set de code page to the appropriate one.\n" <<
		L"For example (Latin 1; Western European - ISO):     chcp 28591\n\n";
	if (extendedHelp)
		wcout <<
		L"The application exposes the \"jThread\" object to the script granting access to the following properties and methods:\n\n" <<
		L"Read-only properties:\n" <<
		L"   version        Returns the jThread version information (string)\n" <<
		L"   name           Returns the name of the jThread object (the host executable file).\n" <<
		L"   path           Returns the name of the directory containing the host executable.\n" <<
		L"   scriptName     Returns the file name of the currently running script.\n" <<
		L"   scriptPath     Returns the absolute path of the currently running script, without the script name.\n" <<
		L"   scriptFullName Returns the full path of the currently running script.\n" <<
		L"   arguments      Returns an Array with the arguments passed to the script\n" <<
		L"   isAsync        Boolean indicating if the script was called in async mode.\n\n" <<
		L"Read-write properties:\n" <<
		L"   currentFolder  Returns/sets the current active folder (string).\n" <<
		L"   debug          Returns/sets the state of the debugger (boolean).\n" <<
		L"   profiler       Returns/sets the log file, empty string to stop (string).\n" <<
		L"   timeout        Returns/sets the timeout for the script (see /T) (number).\n\n" <<
		L"Functions:\n" <<
		L"   echo(string)   Outputs string to the standard output stream (stdOut) of the console.\n" <<
		L"   error(string)  Outputs string to the standard error stream (stdErr) of the console.\n" <<
		L"   input()        Waits for and returns keyboard input (return string). Throws error in async mode.\n" <<
		L"   sleep(time)    Suspends the running script for the specified amount of time (in milliseconds), freeing system resources.\n" <<
		L"                  Other scripts running asynchronously are not affected.\n" <<
		L"   quit(return)   Forces script execution to stop at any time. Optional parameter returned as the process's exit code. \n" <<
		L"                  Other running scripts are not affected.\n" <<
		L"   runScript(scriptPath)\n" <<
		L"                  Loads and runs a script file in the current context, waiting for its execution.\n" <<
		L"                  No return value or parameters passed to the script, as it runs in the same context it shares\n" <<
		L"                  the Global scope and the jThread options.\n\n" <<
		L"Asynchronic functions:\n" <<
		L"   functionAsync(function,hostOptions,arguments...)" <<
		L"   runScriptAsync(scriptPath,argumentsObject,hostOptions)\n" <<
		L"   evalAsync(scriptString,argumentsObject,hostOptions)\n" <<
		L"                  Loads and runs a script file in a new context, returning immediately and running it in a new thread.\n" <<
		L"                  The second alternative instead of loading from file, the script is received as a string.\n" <<
		L"                  The script runs in an independent context, not sharing the Global scope, and with no inter-script\n" <<
		L"                  communication. Information to the new script can be passed thru the jThread object of that script,\n" <<
		L"                  which is initialized with:\n"
		L"                     arguments property cloning a serialized version of argumentsObject.\n" <<
		L"                                        No external or ActiveX objects can be passed.\n" <<
		L"                     debug/profiler/timeout properties taken from hostOptions object.\n" <<
		L"                                        Defaults to the ones of the current script.\n" <<
		L"                  To avoid intermingling output, stdout and stderr of the new script will be buffered and output\n" <<
		L"                  in a block at the end of execution of the calling script.\n"
		L"                  The function returns an object to monitor the state and return value of the script, with the\n" <<
		L"                  following members:\n"
		L"                     isRunning    Boolean indicating if the script is still running.\n" <<
		L"                     runTime      Time in seconds that the script has been running.\n" <<
		L"                     returnValue  An object with a copy of the return value of the script.\n" <<
		L"                                  Will be undefined while executing or if the script doesn't return a value.\n" <<
		L"                                  To return a value the script must use the jThread.quit(returnObject) function.\n" <<
		L"                                  As with parameters the object will be serialized and copied (no external objects).\n" <<
		L"                     exception    If the script finished due to an unhandled exception, the exception doesn't propagate to\n" <<
		L"                                  the calling script, but the corresponding exception object is copied here.\n" <<
		L"                     abort        Aborts the run of the script.\n"
		;

}