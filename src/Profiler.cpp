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

#include "stdafx.h"

#include "Profiler.h"


using namespace std;

Profiler::Profiler(void) : stack(this) , endStack(&stack)
{
	m_refCount = 1;
}

Profiler::~Profiler(void)
{
}

HRESULT Profiler::QueryInterface(REFIID riid, void **ppvObj)
{
	if (riid == IID_IUnknown)
	{
		*ppvObj = (IUnknown *) this;
	}
	else if (riid == IID_IActiveScriptProfilerCallback)
	{
		*ppvObj = (IActiveScriptProfilerCallback *) this;
	}
	else if (riid == IID_IActiveScriptProfilerCallback2)
	{
		*ppvObj = (IActiveScriptProfilerCallback2 *) this;
	}
	else
	{
		*ppvObj = NULL;
		return E_NOINTERFACE;
	}

	AddRef();
	return NOERROR;
}

ULONG Profiler::AddRef()
{
	return InterlockedIncrement(&m_refCount);
}

ULONG Profiler::Release()
{
	long lw;

	if (0 == (lw = InterlockedDecrement(&m_refCount)))
	{
		//delete this;
	}
	return lw;
}

HRESULT Profiler::Initialize(DWORD dwContext)
{
	return S_OK;
}

HRESULT Profiler::Shutdown(HRESULT hrReason)
{
	return S_OK;
}

HRESULT Profiler::ScriptCompiled(PROFILER_TOKEN scriptId, PROFILER_SCRIPT_TYPE type, IUnknown *pIDebugDocumentContext)
{
	scriptList[scriptId] = type;
	return S_OK;
}

HRESULT Profiler::FunctionCompiled(PROFILER_TOKEN functionId, PROFILER_TOKEN scriptId, const wchar_t *pwszFunctionName, const wchar_t *pwszFunctionNameHint, IUnknown *pIDebugDocumentContext)
{
	funcList[{scriptId, functionId}].name = pwszFunctionName;
	return S_OK;
}

HRESULT Profiler::OnFunctionEnter(PROFILER_TOKEN scriptId, PROFILER_TOKEN functionId)
{
	endStack = endStack->newChild(scriptId, functionId);
	return S_OK;
}

HRESULT Profiler::OnFunctionExit(PROFILER_TOKEN scriptId, PROFILER_TOKEN functionId)
{
	endStack = endStack->exit(scriptId, functionId);
	return S_OK;
}

HRESULT Profiler::OnFunctionEnterByName(const wchar_t *pwszFunctionName, PROFILER_SCRIPT_TYPE type)
{
	endStack = endStack->newChild(pwszFunctionName);
	return S_OK;
}

HRESULT Profiler::OnFunctionExitByName(const wchar_t *pwszFunctionName, PROFILER_SCRIPT_TYPE type)
{
	endStack = endStack->exit(pwszFunctionName);
	return S_OK;
}


void Profiler::log() {
	//log call stack
	out << L"Call Stack Profile" << endl;
	stack.log();
	
	out << endl << L"Function Call Summary" << endl;

	//log js function call info
	for (auto child = funcList.begin(); child != funcList.end(); child++)
		if (child->second.count)
			out << child->second.name << L": Count " << child->second.count << L" / Total " << child->second.duration.count() << L"ms" << std::endl;

	//log DOM function call info
	for (auto child = funcListN.begin(); child != funcListN.end(); child++)
		if (child->second.count)
			out << child->first << L": Count " << child->second.count << L" / Total " << child->second.duration.count() << L"ms" << std::endl;

	if (logFile.empty()) 
		wcout << out.rdbuf();
	else
	{
		ofstream log;

		log.open(logFile, ios_base::in | ios_base::out | ios_base::app);
		log << out.rdbuf();
		log.close();
	}


}
