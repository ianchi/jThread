// winUtil.cpp
//
// Author: Adrián Panella
// Description: helper functions to wrap Windows API calls

#include "stdafx.h"
#include "winUtil.h"

using namespace std;

//reads text resource content
wstring getTextResource(wchar_t *res) {
	wostringstream txt;
	HRSRC hRes = FindResource(NULL, res, L"TEXTFILE");
	HGLOBAL hGlobal = NULL;

	if (hRes != NULL)
		hGlobal = LoadResource(NULL, hRes);

	if (hGlobal != NULL) {
		DWORD size = SizeofResource(NULL, hRes);
		const wchar_t *data = (const wchar_t *)LockResource(hGlobal); //it must be in UTF16 LE
		txt.write(data + 1, (size / 2 - 1)); //skip first dword - file UTF16 byte order mark
	}

	return txt.str();

}

bool expandEnvironmentStrings(const wstring& source, wstring& result)
{
	DWORD len;
	len = ExpandEnvironmentStrings(source.c_str(), 0, 0);
	if (len == 0) return false;
	result.resize(len);
	len = ExpandEnvironmentStrings(source.c_str(), &result[0], len);
	if (len == 0) return false;
	result.pop_back(); //Get rid of extra null
	return true;
}

bool expandPath(wstring &source, wstring& dest)
{
	wchar_t str[MAX_PATH];
	auto result = SearchPath(nullptr, source.c_str(), nullptr, MAX_PATH, str, nullptr); //
	if (result == 0 || result > MAX_PATH) return false;

	dest.resize(result);
	dest = str;
	return true;
}

// Convert a wide Unicode string to an ansi string using active code page
string ACP_encode(const wstring &wstr)
{
	int size_needed = WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}



// Convert a string to a wide Unicode String
wstring ACP_decode(const string &str)
{
	int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
	wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

int ACP_decode(const char *str, const int &size, wstring &wstrTo)
{
	int size_needed = MultiByteToWideChar(CP_ACP, 0, str, size, NULL, 0);
	wstrTo.resize(size_needed);
	MultiByteToWideChar(CP_ACP, 0, &str[0], size, &wstrTo[0], size_needed);
	return size_needed;
}

wstring getUser()
{
	wstring name; 
	DWORD size = 0;
	GetUserName(&name[0], &size); 
	name.reserve(size);
	name.resize(size-1);
	GetUserName(&name[0], &size); 
	return name;
}


// Declare Callback Enum Functions.
BOOL CALLBACK TerminateAppEnum(HWND hwnd, LPARAM lParam);

/*----------------------------------------------------------------
DWORD WINAPI TerminateApp( DWORD dwPID, DWORD dwTimeout )

Purpose:
Shut down a 32-Bit Process (or 16-bit process under Windows 95)

Parameters:
dwPID
Process ID of the process to shut down.

dwTimeout
Wait time in milliseconds before shutting down the process.

Return Value:
TA_FAILED - If the shutdown failed.
TA_SUCCESS_CLEAN - If the process was shutdown using WM_CLOSE.
TA_SUCCESS_KILL - if the process was shut down with
TerminateProcess().
NOTE:  See header for these defines.
----------------------------------------------------------------*/
#define TA_FAILED 0
#define TA_SUCCESS_CLEAN 1
#define TA_SUCCESS_KILL 2
#define TA_SUCCESS_16 3


DWORD terminateProcessByID(DWORD dwPID, DWORD dwTimeout)
{
	HANDLE   hProc;
	DWORD   dwRet;

	// If we can't open the process with PROCESS_TERMINATE rights,
	// then we give up immediately.
	hProc = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE,
		dwPID);

	if (hProc == NULL)
	{
		return TA_FAILED;
	}
	dwRet= terminateProcessByHandle(hProc, dwTimeout);
	CloseHandle(hProc);
	
	return dwRet;
}
DWORD terminateProcessByHandle(HANDLE hProc, DWORD dwTimeout)
{
	DWORD   dwRet;
	DWORD	dwPID=GetProcessId(hProc);
	
	// TerminateAppEnum() posts WM_CLOSE to all windows whose PID
	// matches your process's.
	EnumWindows((WNDENUMPROC)TerminateAppEnum, (LPARAM)dwPID);

	// Wait on the handle. If it signals, great. If it times out,
	// then you kill it.
	if (WaitForSingleObject(hProc, dwTimeout) != WAIT_OBJECT_0)
		dwRet = (TerminateProcess(hProc, 0) ? TA_SUCCESS_KILL : TA_FAILED);
	else
		dwRet = TA_SUCCESS_CLEAN;


	return dwRet;
}

BOOL CALLBACK TerminateAppEnum(HWND hwnd, LPARAM lParam)
{
	DWORD dwID;

	GetWindowThreadProcessId(hwnd, &dwID);

	if (dwID == (DWORD)lParam)
	{
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	}

	return TRUE;
}


typedef struct {
	DWORD PID;
	vector<HWND> handles;
} PROCINFO;

BOOL CALLBACK WindowsByProcessIDEnum(HWND hwnd, LPARAM lParam) {
	DWORD dwID;
	PROCINFO *info = (PROCINFO *)lParam;

	GetWindowThreadProcessId(hwnd, &dwID);

	if (dwID == info->PID) info->handles.push_back(hwnd);

	return TRUE;
}


void getWindowsByProcessID(DWORD PID) {
	PROCINFO info;
	info.PID = PID;
	EnumWindows((WNDENUMPROC)WindowsByProcessIDEnum, (LPARAM)&info);


}

//check if string might be UTF8 encoded
bool IsUTF8Bytes(string &data)
{
	int charByteCounter = 1;
	byte curByte;
	for (UINT i = 0; i < data.size(); i++)
	{
		curByte = data[i];
		if (charByteCounter == 1)
		{
			if (curByte >= 0x80)
			{
				while (((curByte <<= 1) & 0x80) != 0)
				{
					charByteCounter++;
				}

				if (charByteCounter == 1 || charByteCounter > 6)
				{
					return false;
				}
			}
		}
		else
		{
			if ((curByte & 0xC0) != 0x80)
			{
				return false;
			}
			charByteCounter--;
		}
	}
	if (charByteCounter > 1)
	{
		return false; //exception:  error byte format
	}
	return true;
}


//autodetect text file encoding
//deduced by BOM, and if not present asumes UTF8 if it is valid char sequence otherwise ANSI 
bool inferEncoding(wstring filename, wifstream &stream) {
	ifstream aux;

	aux.open(filename, ios_base::in | ios::binary, _SH_DENYNO);
	// Read first 4 byte for testing byte order mark (BOM)
	if (!aux.is_open()) return false;

	char buf[] = "\0\0\0\0";
	aux.read(buf, 4);

	int bRead = (int)aux.gcount();

	// no BOM, don't imbue
	if (bRead < 2) return false;

	BYTE utf32_le[] = { 0xFF, 0xFE, 0x00, 0x00 };
	if (memcmp(buf, &utf32_le, 4) == 0) {
		stream.imbue(locale(stream.getloc(), new codecvt_utf16<wchar_t, 0x10ffff, consume_header>));
		return true;
	}

	BYTE utf32_be[] = { 0x00, 0x00, 0xFE, 0xFF };
	if (memcmp(buf, &utf32_be, 4) == 0) {
		stream.imbue(locale(stream.getloc(), new codecvt_utf16<wchar_t, 0x10ffff, consume_header>));
	return true;
}

	BYTE utf_8[] = { 0xEF, 0xBB, 0xBF };
	if (memcmp(buf, &utf_8, 3) == 0) {
		stream.imbue(locale(stream.getloc(), new codecvt_utf8<wchar_t, 0x10ffff, consume_header>));
	return true;
	}

	BYTE utf16_le[] = { 0xFF, 0xFE };
	if (memcmp(buf, &utf16_le, 2) == 0) {
		stream.imbue(locale(stream.getloc(), new codecvt_utf16<wchar_t, 0x10ffff, consume_header>));
	return true;
	}

	BYTE utf16_be[] = { 0xFE, 0xFF };
	if (memcmp(buf, &utf16_be, 2) == 0) {
		stream.imbue(locale(stream.getloc(), new codecvt_utf16<wchar_t, 0x10ffff, consume_header>));
	return true;
	}

	//no BOM, check if it is UTF8 without BOM
	stringstream str;
	str << aux.rdbuf();
	if (IsUTF8Bytes(str.str())) {
		stream.imbue(locale(stream.getloc(), new codecvt_utf8<wchar_t, 0x10ffff, consume_header>));
		return true;
	}

	return false;

}





void readFileANSI(wstring filename, wstring &content) {
	ifstream file;
	stringstream buf;
	file.exceptions(ios::failbit); // errors as exceptions

	file.open(filename, ios_base::in | ios_base::binary, _SH_DENYNO);
		buf << file.rdbuf();
		buf.seekg(0, ios::end);
		int result;
		if (IsTextUnicode(buf.rdbuf(), (int)buf.tellg(), &result)) {
			
		}
		else content=ACP_decode(buf.str());

}


bool readFile(wstring filename, wstring &content) {
	wifstream file;
	wstringstream out;


	file.exceptions(ios::failbit); // errors as exceptions
	try {
		file.open(filename, ios_base::in, _SH_DENYNO);
		inferEncoding(filename, file);
		out << file.rdbuf();
		content = out.str();
		return true;
	}
	catch (const ios_base::failure &f) {
		out << L"Cannot read file " << filename << L" :" << f.what();
		content = out.str();
	}


	return false;
}

/*
* CreateObject
*
* Purpose:
* Creates an instance of the Automation object and obtains it's IDispatch interface.
* Uses Unicode with OLE.
*
* Parameters:
* pszProgID ProgID of Automation object
* ppdisp Returns IDispatch of Automation object
*
* Return Value:
* HRESULT indicating success or failure
*/
HRESULT CreateObject(wstring progID, IDispatch FAR* FAR* ppdisp)
{
	CLSID clsid; // CLSID of automation object
	HRESULT hr;
	LPUNKNOWN punk = NULL; // IUnknown of automation object 
	LPDISPATCH pdisp = NULL; // IDispatch of automation object 

	*ppdisp = NULL;

	hr=CoInitialize(nullptr);
	// Retrieve CLSID from the progID that the user specified 
	hr = CLSIDFromProgID(progID.c_str(), &clsid);
	if (FAILED(hr))
		goto error;

	// Create an instance of the automation object and ask for the IDispatch interface 
	hr = CoCreateInstance(clsid, NULL, CLSCTX_SERVER,
		IID_IUnknown, (void FAR* FAR*)&punk);
	if (FAILED(hr))
		goto error;

	hr = punk->QueryInterface(IID_IDispatch, (void FAR* FAR*)&pdisp);
	if (FAILED(hr))
		goto error;

	*ppdisp = pdisp;
	punk->Release();
	return NOERROR;

error:
	if (punk) punk->Release();
	if (pdisp) pdisp->Release();
	return hr;
}



wstring getFullPathName(const wstring &path) {
	DWORD len = GetFullPathName(path.c_str(), 0, nullptr, nullptr);
	wstring fullName;
	fullName.resize(len - 1);
	fullName.reserve(len);
	GetFullPathName(path.c_str(), len, (LPWSTR)fullName.c_str(), nullptr);

	return fullName;
}


wstring getParentFolder(const wstring &path) {
	wregex regexStr(L"^(.*)\\\\([^\\\\]+)\\\\?$", regex::ECMAScript); //valid modified options
	wsmatch	res;
	return regex_search(path, res, regexStr) ? res[1].str() : L"";
}

wstring getFilename(const wstring &path) {
	wregex regexStr(L"([^\\\\]+)$", regex::ECMAScript); //valid modified options
	wsmatch	res;
	return regex_search(path, res, regexStr) ? res[1].str() : L"";
}

bool createDirectories(const wstring &path) {
	using namespace std::tr2::sys;

	return create_directories(wpath(getParentFolder(path)));
}

wstring getLastErrorMsg() {
	DWORD err = GetLastError();
	
	wchar_t *pMsg=nullptr;
	auto ret = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, 0, (LPWSTR)&pMsg, 0, nullptr);
	wstring msg(pMsg);
	LocalFree(pMsg);

	return msg;
}