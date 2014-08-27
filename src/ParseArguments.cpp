#include "stdafx.h"
#include "ParseArguments.h"


using namespace std;

ParseArguments::ParseArguments(const wstring &bList, std::initializer_list<std::pair<wchar_t, std::wstring>> sList, int argc, wchar_t* argv[]) :
boolList(bList), parseOK(false), strPattern(sList.begin(), sList.end()), strList(L"")
{
	//generates string of valid options
	for (auto item = sList.begin(); item != sList.end(); item++)
		strList.append(1,item->first);

	Parse(argc, argv);
}

ParseArguments::ParseArguments(const wstring &bList, const wstring &sList, int argc, wchar_t* argv[]) :
boolList(bList), strList(sList), parseOK(false)
{
	Parse(argc, argv);
}

void ParseArguments::Parse(int argc, wchar_t* argv[])
{
	wstring	argument, modifier;
	wregex regexBool(L"^/([" + boolList + L"])$", regex::ECMAScript);  //valid boolean options
	wregex regexStr(L"^/([" + strList + L"]):(.+)$", regex::icase & regex::ECMAScript); //valid modified options
	wregex regexPattern;
	wsmatch	res;
	enum class groupEnum { opciones, script, params };
	groupEnum group = groupEnum::opciones;
	wchar_t opt = L'\0';
	//initilize options. Boolean defaults to false
	for (UINT i = 0; i < boolList.size(); i++)
		boolOpts[boolList[i]] = false;

	if (argc <= 1) return;
	for (int i = 1; i < argc; i++) {
		argument = argv[i];

		if (group == groupEnum::opciones) {
			if (boolList.size()>0) {
				//valid options without modifiers
				if (regex_search(argument, res, regexBool)) { //it is valid
					boolOpts[res[1].str()[0]] = true;
					continue;
				}
			}

			if (strList.size()>0) {
				//valid options with modifiers
				if (regex_search(argument, res, regexStr)) { //it is valid
					opt = res[1].str()[0];
					modifier = res[2].str();
					//check validity of modifier
					regexPattern.assign(L"^" + strPattern[opt] + L"$");
					if ((strPattern[opt]).empty() || regex_search(modifier, res, regexPattern))
						strOpts[opt] = modifier;
					else return; //invalid pattern
					continue;
				}
			}
			//first non-option should be script name
			group = groupEnum::script;

			//check valid path sintax
			regexPattern.assign(L"^([a-z]:)?([\\w\\-\\s\\.\\\\]+)$", regex::icase & regex::ECMAScript);
			if (regex_search(argument, res, regexPattern)) {
				tr2::sys::wpath aux = argument;

				name = aux.leaf();
				path = tr2::sys::complete(aux).parent_path();
				group = groupEnum::params;
				continue;
			}
			else return; //invalid switch
		}
		else { //siguen los parametros
			params.push_back(argument);
		}

	}
	parseOK = true;
}
