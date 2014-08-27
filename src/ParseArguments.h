#pragma once


struct ParseArguments {
	std::map<wchar_t, bool> boolOpts;
	std::map<wchar_t, std::wstring> strOpts;
	std::map<wchar_t, std::wstring> strPattern;
	std::wstring name;
	std::wstring path;
	std::vector<std::wstring> params;

	bool parseOK;
	ParseArguments(const std::wstring &bList, const std::wstring &sList, int argc, wchar_t* argv[]);
	ParseArguments(const std::wstring &bList, std::initializer_list<std::pair<wchar_t,std::wstring>> sList, int argc, wchar_t* argv[]);
	void Parse(int argc, wchar_t* argv[]);
private:
	std::wstring boolList;
	std::wstring strList;

};
