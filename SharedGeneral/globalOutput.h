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

	static void WriteLocked(std::string str, bool newline = false)
	{
		if (newline)
		{
			WriteLocked([&str]() { std::cout << str << std::endl; });
		}
		else
		{
			WriteLocked([&str]() { std::cout << str; });
		}
	}

	static void WriteLocked(std::wstring wstr, bool newline = false)
	{
		if (newline)
		{
			WriteLocked([&wstr]() { std::wcout << wstr << std::endl; });
		}
		else
		{
			WriteLocked([&wstr]() { std::wcout << wstr; });
		}
	}

private:
	static std::mutex lock;
};

#endif // GLOBAL_OUTPUT_H