#pragma once

#include "jsrt-wrappers\jsrt-proxy_object.h"

class file_object
{
private:
	std::wstring filePath;
	std::wfstream file;
	jsrt::proxy_object<file_object> jsObject;



public:
	file_object(std::wstring path);
	~file_object();

	jsrt::value	createjsObject();

	void open(jsrt::optional<std::wstring> openMode, jsrt::optional<std::wstring> shareMode, jsrt::optional<std::wstring> encoding);
	void create(jsrt::optional<std::wstring> openMode, jsrt::optional<std::wstring> shareMode, jsrt::optional<std::wstring> encoding);
	bool isOpen() {
		return file.is_open();
	}
	void close() {
		file.close();
	}
	bool eof() {
		return file.eof();
	}

	std::wstring readAll() {
		std::wstringstream buf;
		buf << file.rdbuf();
		return buf.str();

	}
	std::wstring readLine() {
		std::wstring line;
		std::getline(file, line);
		return line;
	}

	std::wstring read(jsrt::optional<int> count) {
		std::wstring data;
		if (count.has_value()) {
			data.reserve(count.value());
			file.read((wchar_t *)data.c_str(),count.value());
			data.resize(count.value());
		}
		else return readAll();

		return data;
	}

	void writeLine(std::wstring line) {
		file << line << std::endl;
	}
	void write(std::wstring data) {
		file << data;
	}
};

