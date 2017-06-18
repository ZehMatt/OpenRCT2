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

typedef enum LogCategory {
	LOG_GROUP_GENERAL = 0,
	LOG_GROUP_SERVER,
	LOG_GROUP_CLIENT,
} LogCategory;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

	void log_init(LogCategory cat, uint32 historySize, const char *logFile);
	void log_rotate(LogCategory cat);
	void log_flush(LogCategory cat);
	void log_flush_all();
	void log_msg(LogCategory cat, const char *fmt, ...);
	void log_msg_network(const char *fmt, ...);

#ifdef __cplusplus
}
#endif // __cplusplus