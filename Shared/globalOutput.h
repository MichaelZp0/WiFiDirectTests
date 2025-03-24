#ifndef GLOBAL_OUTPUT_H
#define GLOBAL_OUTPUT_H

#include "pch.h"

#include <iostream>
#include <mutex>
#include <functional>

class GlobalOutput
{
public:
	static void WriteLocked(std::function<void()> writeFunc)
	{
		std::lock_guard<std::mutex> lk(GlobalOutput::lock);
		writeFunc();
	}

	static void WriteLocked(std::string str)
	{
		WriteLocked([&str]() { std::cout << str; });
	}

	static void WriteLocked(std::wstring wstr)
	{
		WriteLocked([&wstr]() { std::wcout << wstr; });
	}

private:
	static std::mutex lock;
};

#endif // GLOBAL_OUTPUT_H