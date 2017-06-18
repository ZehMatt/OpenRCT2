#include "Logging.hpp"
#include "Logging.h"
#include "../network/network.h"

extern "C"
{
#include "../platform/platform.h"
}

Logging gLogging;

Logging::Logging()
{
}

void Logging::initLog(LogCategory cat, size_t historySize, const char *logFile)
{
	GroupData& group = _groups[cat];
	group.entries.reset(historySize);
	group.counter = platform_get_ticks();
	group.rotation = 0;
	strcpy_s(group.logFileName, logFile);
}

void Logging::flushCategory(LogCategory cat)
{
	auto it = _groups.find(cat);
	if (it == _groups.end())
		return;

	flushCategory(it->second);
}

void Logging::flushCategory(GroupData& group)
{
	char outFile[255];
	if(group.rotation > 0)
		sprintf_s(outFile, "%zd_%s", group.rotation, group.logFileName);

	FILE *fpOut = fopen(group.rotation > 0 ? outFile : group.logFileName, "wt");
	if (!fpOut)
		return;

	for (size_t n = 0; n < group.entries.size(); n++)
	{
		const MessageEntry& entry = group.entries[n];
		fprintf(fpOut, "[T:%08X, R:%04X] ", entry.tick, entry.repeat);
		fputs(entry.msg.c_str(), fpOut);
	}

	fclose(fpOut);
}

void Logging::flushAll()
{
	for (auto& it : _groups)
	{
		flushCategory(it.second);
	}
}

void Logging::rotate(LogCategory cat)
{
	auto it = _groups.find(cat);
	if (it == _groups.end())
		return;

	GroupData& group = it->second;
	flushCategory(group);

	group.rotation++;
}

void Logging::log(LogCategory cat, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	log_va(cat, fmt, args);
	va_end(args);
}

void Logging::log_va(LogCategory cat, const char *fmt, va_list args)
{
	auto it = _groups.find(cat);
	if (it == _groups.end())
		return;

	GroupData& group = it->second;

	char messageBuf[4048];
	vsprintf_s(messageBuf, fmt, args);

	std::string msgStr = messageBuf;

	size_t lastEntry = group.entries.size();
	bool updated = false;

	if (lastEntry != 0)
	{
		MessageEntry& lastMsg = group.entries[lastEntry - 1];
		if (lastMsg.tick == gCurrentTicks && lastMsg.msg == msgStr)
		{
			lastMsg.repeat++;
			updated = true;
		}
	}

	if (!updated)
	{
		MessageEntry& msg = group.entries.occupy();
		msg.msg = std::move(msgStr);
		msg.timestamp = time(nullptr);
		msg.tick = gCurrentTicks;
		msg.repeat = 0;
	}

	uint32 curTicks = platform_get_ticks();
	if (curTicks - group.counter >= 1000)
	{
		flushCategory(group);
		group.counter = curTicks;
	}
}

extern "C"
{
	void log_init(LogCategory cat, uint32 historySize, const char *logFile)
	{
		gLogging.initLog(cat, (size_t)historySize, logFile);
	}

	void log_msg(LogCategory cat, const char *fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		gLogging.log_va(cat, fmt, args);
		va_end(args);
	}

	void log_flush(LogCategory cat)
	{
		gLogging.flushCategory(cat);
	}

	void log_flush_all()
	{
		gLogging.flushAll();
	}

	void log_msg_network(const char *fmt, ...)
	{
		LogCategory cat = LOG_GROUP_GENERAL;

		if (network_get_mode() == NETWORK_MODE_CLIENT)
			cat = LOG_GROUP_CLIENT;
		else if (network_get_mode() == NETWORK_MODE_SERVER)
			cat = LOG_GROUP_SERVER;

		va_list args;
		va_start(args, fmt);
		gLogging.log_va(cat, fmt, args);
		va_end(args);
	}


	void log_rotate(LogCategory cat)
	{
		gLogging.rotate(cat);
	}

}
