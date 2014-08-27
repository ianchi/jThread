#pragma once

// winUtil.cpp
//
// Author: Adrián Panella
// Description: helper functions to wrap Windows API calls


std::wstring getTextResource(wchar_t *res);
bool expandEnvironmentStrings(const std::wstring& source, std::wstring& result);
bool expandPath(std::wstring &source, std::wstring& dest);

std::wstring ACP_decode(const std::string &str);
std::string ACP_encode(const std::wstring &wstr);
int ACP_decode(const char *str, const int &size, std::wstring &wstrTo);

std::wstring getUser();

DWORD terminateProcessByID(DWORD dwPID, DWORD dwTimeout);
DWORD terminateProcessByHandle(HANDLE hProc, DWORD dwTimeout);

void getWindowsByProcessID(DWORD PID);

bool readFile(std::wstring filename, std::wstring &content);
bool readFileUTF16(std::wstring filename, std::wstring &content);
