// Copyright 2014 Adrian Panella
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// \mainpage jThread - JavaScript Runtime Host with Multithreading 

/// \file
/// \brief Provides interface for profiling js scripts

#pragma once


class Profiler sealed : public IActiveScriptProfilerCallback2
{
private:
	long m_refCount;

	struct FunctionStack {
		std::pair<PROFILER_TOKEN, PROFILER_TOKEN> functionID;
		std::wstring functionName;
		int level;

		std::chrono::time_point<std::chrono::system_clock> startTime, endTime;

		std::chrono::milliseconds total;
		std::chrono::milliseconds included;

		std::vector<FunctionStack> children;
		FunctionStack *parent;
		Profiler *profiler;

		FunctionStack(Profiler *prof) : profiler(prof),
			startTime(std::chrono::system_clock::now()), endTime(startTime), parent(nullptr), level(0), included(0)
		{

		}

		FunctionStack *newChild(PROFILER_TOKEN script, PROFILER_TOKEN func) {
			FunctionStack child(profiler);
			child.parent = this;
			child.level = level + 1;
			child.functionID = { script, func };
			child.functionName = profiler->funcList[child.functionID].name;

			children.push_back(child);
			return &(children.back());
		}
		FunctionStack *newChild(std::wstring func) {
			FunctionStack child(profiler);
			child.parent = this;
			child.level = level + 1;
			child.functionName = func;

			children.push_back(child);
			return &(children.back());
		}

		FunctionStack *exit(PROFILER_TOKEN script, PROFILER_TOKEN func)
		{
			//check consistency of function !!!

			endTime = std::chrono::system_clock::now();
			total = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
			parent->included += total;

			profiler->funcList[functionID].count++;
			profiler->funcList[functionID].duration += total;

			return parent;
		}

		FunctionStack *exit(std::wstring func)
		{
			//check consistency of function !!!

			endTime = std::chrono::system_clock::now();
			total = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
			parent->included += total;

			profiler->funcListN[func].count++;
			profiler->funcListN[func].duration += total;

			return parent;
		}

		long long getDuration() {
			return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
		}

		void log() {
			if (level!=0)
				profiler->out << std::wstring(level-1,L'\t') << functionName << L": Total " << total.count() << L"ms / Exclusive " << total.count() - included.count() << L"ms" << std::endl;

			for (auto child = children.begin(); child != children.end(); child++)
				(*child).log();

		}
	};

	struct FunctionInfo {
		std::wstring name;
		UINT count{ 0 };
		std::chrono::milliseconds duration{ 0 };
	};

	std::map<std::pair<PROFILER_TOKEN, PROFILER_TOKEN>, FunctionInfo> funcList;
	std::map<std::wstring, FunctionInfo> funcListN;
	std::map<PROFILER_TOKEN, PROFILER_SCRIPT_TYPE> scriptList;
	
	FunctionStack stack;
	FunctionStack *endStack;

	std::wstringstream out;
	std::wstring logFile;

public:
	Profiler(void);
	~Profiler(void);

	void log();

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

	// IActiveScriptProfilerCallback2 - called on DOM
	HRESULT STDMETHODCALLTYPE OnFunctionEnterByName(const wchar_t *pwszFunctionName, PROFILER_SCRIPT_TYPE type);
	HRESULT STDMETHODCALLTYPE OnFunctionExitByName(const wchar_t *pwszFunctionName, PROFILER_SCRIPT_TYPE type);
};
