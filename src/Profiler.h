#pragma once


class Profiler sealed : public IActiveScriptProfilerCallback2
{
private:
	long m_refCount;

	struct FunctionStack {
		std::pair<PROFILER_TOKEN, PROFILER_TOKEN> function;
		int level;

		std::chrono::time_point<std::chrono::system_clock> startTime, endTime;

		std::chrono::milliseconds total;
		std::chrono::milliseconds included;

		std::vector<FunctionStack> children;
		FunctionStack *parent;

		FunctionStack() :
			startTime(std::chrono::system_clock::now()), endTime(startTime), parent(nullptr), level(0), included(0)
		{

		}

		FunctionStack *newChild(PROFILER_TOKEN script, PROFILER_TOKEN func) {
			FunctionStack child;
			child.parent = this;
			child.level = level + 1;
			child.function = { script, func };

			children.push_back(child);
			return &(children.back());
		}

		FunctionStack *exit(PROFILER_TOKEN script, PROFILER_TOKEN func)
		{
			//check consistency of function !!!

			endTime = std::chrono::system_clock::now();
			total = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
			parent->included += total;

			return parent;
		}

		long long getDuration() {
			return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
		}

		void log(Profiler *profiler) {
			if (level!=0)
				std::wcout << std::wstring(level-1,L'\t') << profiler->funcList[function] << L": " << total.count() << L" / " << total.count() - included.count() << std::endl;

			for (auto child = children.begin(); child != children.end(); child++)
				(*child).log(profiler);
			

		}


	};

	std::map<std::pair<PROFILER_TOKEN, PROFILER_TOKEN>, std::wstring> funcList;
	std::map<PROFILER_TOKEN, PROFILER_SCRIPT_TYPE> scriptList;
	
	FunctionStack stack;
	FunctionStack *endStack;


public:
	Profiler(void);
	~Profiler(void);

	void log() { 
		stack.log(this); }

	// IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj);
	ULONG STDMETHODCALLTYPE AddRef(void);
	ULONG STDMETHODCALLTYPE Release(void);

	// IActiveScriptProfilerCallback
	HRESULT STDMETHODCALLTYPE Initialize(DWORD dwContext);
	HRESULT STDMETHODCALLTYPE Shutdown(HRESULT hrReason);
	HRESULT STDMETHODCALLTYPE ScriptCompiled(PROFILER_TOKEN scriptId, PROFILER_SCRIPT_TYPE type, IUnknown *pIDebugDocumentContext);
	HRESULT STDMETHODCALLTYPE FunctionCompiled(PROFILER_TOKEN functionId, PROFILER_TOKEN scriptId, const wchar_t *pwszFunctionName, const wchar_t *pwszFunctionNameHint, IUnknown *pIDebugDocumentContext);
	HRESULT STDMETHODCALLTYPE OnFunctionEnter(PROFILER_TOKEN scriptId, PROFILER_TOKEN functionId);
	HRESULT STDMETHODCALLTYPE OnFunctionExit(PROFILER_TOKEN scriptId, PROFILER_TOKEN functionId);

	// IActiveScriptProfilerCallback2
	HRESULT STDMETHODCALLTYPE OnFunctionEnterByName(const wchar_t *pwszFunctionName, PROFILER_SCRIPT_TYPE type);
	HRESULT STDMETHODCALLTYPE OnFunctionExitByName(const wchar_t *pwszFunctionName, PROFILER_SCRIPT_TYPE type);
};
