#include "stdafx.h"
#include "ConsoleLogWriter.h"
#include <iostream>

namespace CommonLib
{
	void CConsoleLogWriter::Write(const astr& msg)
	{
		std::cout << msg.c_str();
	}
}