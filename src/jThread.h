
#pragma once
#ifndef JTHREAD_H
#define JTHREAD_H

//variables globales
#define GET_SELF JSRuntimeInstance* self = static_cast<JSRuntimeInstance*>(jsrt::external_object(info.this_value()).data())



enum scriptMode : unsigned int { //tres bits: async | eval | buffered stdErr
	main = 0,
	async = 1,
	eval = 2,
	buffer = 4
};

enum timeMessageMode { start, end, dura };
enum instanceState { notRun, running, idle };

wstring logTimeMessage(chrono::time_point<chrono::system_clock> &start, chrono::time_point<chrono::system_clock> &end, wstring msg, timeMessageMode mode);

class GlobalHostOptions;
class ExecProcess;

class HostOptions {
public:
	bool debug;
	bool verbose;
	bool help;
	wstring profile;
	wstring outFile;
	wstring errFile;
	int timeoutMins;
	wstring scriptName, scriptPath, script;
	vector<wstring> scriptArguments;
	bool parseOK;

	HostOptions(GlobalHostOptions &globalObj) :
		debug(false), profile(L""), scriptName(L""), verbose(false), help(false),
		outFile(L""), errFile(L""), timeoutMins(0), parseOK(false)
	{
	};
	HostOptions(int argc, wchar_t* argv[])  {
		parseArguments(argc, argv);
	};

	HostOptions(HostOptions &opts, wstring script) :
		debug(opts.debug), verbose(opts.verbose), help(opts.help),
		profile(opts.profile), outFile(opts.outFile), errFile(opts.errFile),
		timeoutMins(opts.timeoutMins), parseOK(opts.parseOK)	{
		//ajusta script y parametro
		tr2::sys::wpath path = script;
		scriptName = path.leaf();
		scriptPath = tr2::sys::complete(path).parent_path();
		
	};
	bool parseArguments(int argc, wchar_t* argv[]) {
		wstring	argument, opt, file;
		wregex regexOpt(L"^/([DPOETV?])(:.*)?$", regex::ECMAScript);  //valid options
		wsmatch	res;
		bool result = true;
		int grupo = 0; //0: opciones 1:script 2:params

		if (argc <= 1) result = false;
		for (int i = 1; i < argc; i++) {
			argument = argv[i];

			if (grupo == 0 && regex_search(argument, res, regexOpt)) { // es una opción valida
				opt = res[1].str();
				file = res[2].str();

				if (opt == L"D") debug = true;
				else if (opt == L"V") verbose = true;
				else if (opt == L"?") help = true;
				else if (opt == L"P") {
					if (!file.empty()) profile = file;
					else profile = L"stdout";
				}
				else if (opt == L"O") {
					if (file.empty()) result = false;
					else outFile = file;
				}
				else if (opt == L"E") {
					if (file.empty()) result = false;
					else errFile = file;
				}
				else if (opt == L"T") {
					if (file.empty()) result = false;
					else timeoutMins = _wtoi(file.c_str());
				}
			}
			else if (grupo == 0) { //primero que no es una opcion => deberia ser el script
				grupo = 1;
				//validar que "parezca" una ruta valida
				regexOpt.assign(L"^([a-z]:)?([\\w\\-\\s\\.\\\\]+)$", regex::icase & regex::ECMAScript);
				if (regex_search(argument, res, regexOpt)) {
					tr2::sys::wpath path = argument;

					scriptName = path.leaf();
					scriptPath = tr2::sys::complete(path).parent_path();
				}
				else result = false;
			}
			else { //siguen los parametros
				scriptArguments.push_back(argument);
			}

		}
		parseOK = result;
		return result;
	}
};

class JSRuntimeInstance {
public:
	HostOptions hostOpt;
	unsigned int mode;
	//wostringstream stdOut, stdErr; makes the object not movable
	unique_ptr<wstringstream> stdOut, stdErr;
	chrono::time_point<chrono::system_clock> startTime, endTime;
	instanceState state;
	mutex runningMutex;

	jsrt::runtime runtime;
	jsrt::context context;
	wstring scriptResult;

	future<void> futureReturn;

	vector<unique_ptr<ExecProcess>> childProcess;


	void createjThreadObject();

	JSRuntimeInstance(HostOptions &opts, unsigned int runMode = scriptMode::main) : hostOpt(opts), mode(runMode),
		stdOut(make_unique<wstringstream>()), stdErr(make_unique<wstringstream>()), state(instanceState::notRun)
	{
		
	};

	~JSRuntimeInstance() { 
		
		flush(); }
	void operator() (){ run(); }

	void run(); //crea la instancia de JsRt y ejecuta el script según las opciones

	void asyncRun() {
		futureReturn = std::async(launch::async, bind(&JSRuntimeInstance::run, this));
	}

	void error(wstring msg) { //escribe en el stream de error o en el buffer, según corresponda
		*stdErr << msg;
		if (!(mode & scriptMode::async || mode & scriptMode::buffer)) 
			wcerr << msg;
	};

	void echo(wstring msg) { //escribe en el stream de error o en el buffer, según corresponda
		*stdOut << msg;
		if (!(mode & scriptMode::async))
			wcout << msg;
	}

	void raise(wstring msg) { //genera una excepción en el script o escribe en error stream, según corresponda
		if(mode & scriptMode::async)
			error(msg);
		else
			jsrt::context::set_exception(jsrt::error::create(msg));
	};

	void flush() { //escribe el buffer en los stream de la consola
		if (mode & scriptMode::async) {
			if (hostOpt.verbose) wcout << logTimeMessage(startTime, endTime, L" - Start script " + hostOpt.scriptName, timeMessageMode::start) << endl;
			wcout << stdOut->rdbuf();
			if (hostOpt.verbose) wcout << logTimeMessage(startTime, endTime, L" - End script " + hostOpt.scriptName, timeMessageMode::end) << endl;
		}
		if (mode & scriptMode::async || mode & scriptMode::buffer) {
			wcerr << stdErr->rdbuf();
			if ((int)(stdErr->tellp()) != 0 && hostOpt.verbose) wcerr << "End script " << hostOpt.scriptName << " errors.\n";
		}
	}

	static wstring jsStringify(jsrt::value param);
	static jsrt::value jsParse(wstring param);
	static JSRuntimeInstance* getSelfPointer(const jsrt::call_info &info);
	jsrt::object createAsyncObject();



	template<class T>
	static jsrt::property_id createValueProp(jsrt::object &destObj, const wstring &propName, const T& propValue, 
		bool isWritable = false, bool isEnumerable = true, bool isConfigurable = false)
	{
		jsrt::property_descriptor<T> descriptor = jsrt::property_descriptor<T>::create();
		jsrt::property_id propId = jsrt::property_id::create(propName);
		descriptor.set_configurable(isConfigurable);
		descriptor.set_enumerable(isEnumerable);
		descriptor.set_writable(isWritable);
		descriptor.set_value(propValue);
		destObj.define_property(propId, descriptor);
		return propId;
	}

	template<class T>
	void createGetSetProp(jsrt::object &destObj, const wstring &propName, T(*getter)(const jsrt::call_info &),
		void(*setter)(const jsrt::call_info &, T), bool isEnumerable = true, bool isConfigurable = false)
	{
		jsrt::property_descriptor<T> descriptor = jsrt::property_descriptor<T>::create();
		jsrt::property_id propId = jsrt::property_id::create(propName);
		descriptor.set_configurable(isConfigurable);
		descriptor.set_enumerable(isEnumerable);
		if (getter != nullptr) descriptor.set_getter(jsrt::function<T>::create(getter));
		if (setter != nullptr) descriptor.set_setter(jsrt::function<void,T>::create(setter));
		destObj.define_property(propId, descriptor);
	}

	//variable argument list
	void createFunctionProp(jsrt::object &destObj, const wstring &propName, jsrt::value(*lambda)(const jsrt::call_info &, const vector<jsrt::value> &),
		bool isWritable = false, bool isEnumerable = true, bool isConfigurable = false)
	{
		jsrt::function_base  callBack = jsrt::function_base::create(lambda);
		jsrt::property_descriptor<jsrt::function_base> descriptor = jsrt::property_descriptor<jsrt::function_base>::create();
		jsrt::property_id propId = jsrt::property_id::create(propName);
		descriptor.set_configurable(isConfigurable);
		descriptor.set_enumerable(isEnumerable);
		descriptor.set_writable(isWritable);

		descriptor.set_value(callBack);
		destObj.define_property(propId, descriptor);
	}

	//no parameteres
	template<class R>
	void createFunctionProp(jsrt::object &destObj, const wstring &propName, R(*lambda)(const jsrt::call_info &),
		bool isWritable = false, bool isEnumerable = true, bool isConfigurable = false)
	{
		jsrt::function<R> callBack = jsrt::function<R>::create(lambda);
		jsrt::property_descriptor<jsrt::function<R>> descriptor = jsrt::property_descriptor<jsrt::function<R>>::create();
		jsrt::property_id propId = jsrt::property_id::create(propName);
		descriptor.set_configurable(isConfigurable);
		descriptor.set_enumerable(isEnumerable);
		descriptor.set_writable(isWritable);
		descriptor.set_value(callBack);
		destObj.define_property(propId, descriptor);
	}

	//definite parameters
	template<class R, class... P>
	void createFunctionProp(jsrt::object &destObj, const wstring &propName, R(*lambda)(const jsrt::call_info &, P...),
		bool isWritable = false, bool isEnumerable = true, bool isConfigurable = false)
	{
		jsrt::function<R, P...> callBack = jsrt::function<R, P...>::create(lambda);
		jsrt::property_descriptor<jsrt::function<R, P...>> descriptor = jsrt::property_descriptor<jsrt::function<R, P...>>::create();
		jsrt::property_id propId = jsrt::property_id::create(propName);
		descriptor.set_configurable(isConfigurable);
		descriptor.set_enumerable(isEnumerable);
		descriptor.set_writable(isWritable);
		descriptor.set_value(callBack);
		destObj.define_property(propId, descriptor);
	}


};

class GlobalHostOptions {
	public:
	vector<unique_ptr<JSRuntimeInstance>> jsRTinstances;

	wstring version;
	wstring name; //           Returns the name of the jThread object(the host executable file).\n" <<
	wstring path; //           Returns the name of the directory containing the host executable.\n" <<

	JSRuntimeInstance* newInstance(HostOptions &opts, unsigned int async = scriptMode::main) {
		unique_ptr<JSRuntimeInstance> rtm = make_unique<JSRuntimeInstance>(opts, async);
		jsRTinstances.push_back(move(rtm));
		JSRuntimeInstance* ret = (jsRTinstances.back()).get();
		return ret;
	}
	GlobalHostOptions(){}
};


//global functions
void showHelp(bool extendedHelp);
bool readFile(wstring filename, wstring &content);
void createRuntime(HostOptions &args);

#endif // !JTHREAD_H
