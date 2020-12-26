#pragma once

#include <stdio.h>

void sp_log(const char* format, ...)
{
	char buf[8192];

	va_list args;
	va_start(args, format);
	const UINT size = vsprintf_s(buf, format, args);
	sprintf_s(buf, "%s%c", buf, '\n');
	va_end(args);

	OutputDebugStringA(buf);
}