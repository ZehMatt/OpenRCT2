#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
* OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
*
* OpenRCT2 is the work of many authors, a full list can be found in contributors.md
* For more information, visit https://github.com/OpenRCT2/OpenRCT2
*
* OpenRCT2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* A full copy of the GNU General Public License can be found in licence.txt
*****************************************************************************/
#pragma endregion

#pragma once

#include "Logging.h"
#include <map>
#include <string>

#include "../common.h"
#include "HistoryBuffer.h"

class Logging
{
private:
	struct MessageEntry
	{
		std::string msg;
		uint32 tick;
		uint32 repeat;
		time_t timestamp;
	};

	struct GroupData
	{
		char logFileName[255];
		uint32 counter;
		uint32 rotation;
		HistoryBuffer<MessageEntry> entries;
	};

	std::map<LogCategory, GroupData> _groups;

public:
	Logging();

	void initLog(LogCategory cat, size_t historySize, const char *logFile);
	void flushCategory(LogCategory cat);
	void flushCategory(GroupData& group);

	void flushAll();
	void rotate(LogCategory cat);
	void log(LogCategory cat, const char *fmt, ...);
	void log_va(LogCategory cat, const char *fmt, va_list args);
};