/*
 * Logger
 * Splits log lines into records.
 */


#include "LogParser.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>


namespace {


enum {
	kAnyCase	= 0x01,
		// in free text the word counts in any case, not only in capitals
	kTextOnly	= 0x02
		// the word is no level of its own in front of a message
};

struct LevelWord {
	const char*	word;
	int32		severity;
	uint32		flags;
};

const LevelWord kLevelWords[] = {
	{ "emerg", kSeverityEmergency, 0 },
	{ "emergency", kSeverityEmergency, 0 },
	{ "panic", kSeverityEmergency, kAnyCase },
	{ "alert", kSeverityAlert, 0 },
	{ "crit", kSeverityCritical, 0 },
	{ "critical", kSeverityCritical, kAnyCase },
	{ "fatal", kSeverityCritical, kAnyCase },
	{ "err", kSeverityError, kAnyCase },
	{ "error", kSeverityError, kAnyCase },
	{ "severe", kSeverityError, 0 },
	{ "failed", kSeverityError, kAnyCase | kTextOnly },
	{ "failure", kSeverityError, kAnyCase | kTextOnly },
	{ "warn", kSeverityWarning, kAnyCase },
	{ "warning", kSeverityWarning, kAnyCase },
	{ "notice", kSeverityNotice, 0 },
	{ "info", kSeverityInfo, 0 },
	{ "information", kSeverityInfo, 0 },
	{ "debug", kSeverityDebug, 0 },
	{ "trace", kSeverityDebug, 0 },
	{ "verbose", kSeverityDebug, 0 }
};

const char* kMonths[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// syslog facilities by number, as Haiku's syslog_daemon names them.
const char* kFacilities[] = {
	"KERN", "USER", "MAIL", "DAEMON", "AUTH", "SYSLOG", "LPR", "NEWS",
	"UUCP", "CRON", "AUTHPRIV", "FTP", "NTP", "SECURITY", "CONSOLE",
	"CLOCK", "LOCAL0", "LOCAL1", "LOCAL2", "LOCAL3", "LOCAL4", "LOCAL5",
	"LOCAL6", "LOCAL7"
};


#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))


inline bool
IsDigit(char c)
{
	return c >= '0' && c <= '9';
}


inline bool
IsAlpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}


inline bool
IsWordChar(char c)
{
	return IsDigit(c) || IsAlpha(c) || c == '_';
}


inline void
SkipSpaces(const char*& p)
{
	while (*p == ' ' || *p == '\t')
		p++;
}


int32
LevelForWord(const char* word, int32 length, bool inText)
{
	for (size_t i = 0; i < COUNT_OF(kLevelWords); i++) {
		const LevelWord& level = kLevelWords[i];
		if ((int32)strlen(level.word) != length
			|| strncasecmp(word, level.word, length) != 0) {
			continue;
		}

		if (!inText) {
			return (level.flags & kTextOnly) != 0
				? kSeverityNone : level.severity;
		}
		if ((level.flags & kAnyCase) != 0)
			return level.severity;
		for (int32 j = 0; j < length; j++) {
			if (!isupper((unsigned char)word[j]))
				return kSeverityNone;
		}
		return level.severity;
	}
	return kSeverityNone;
}


// "<PRI>"; returns the priority, or -1 when there is none.
int32
ParsePriority(const char*& s)
{
	if (*s != '<')
		return -1;

	const char* p = s + 1;
	int32 value = 0;
	int32 digits = 0;
	while (IsDigit(*p) && digits < 3) {
		value = value * 10 + (*p - '0');
		p++;
		digits++;
	}
	if (digits == 0 || *p != '>' || value > 191)
		return -1;

	s = p + 1;
	return value;
}


// "hh:mm:ss" with optional fractions of a second.
bool
ParseClock(const char*& p)
{
	if (!IsDigit(p[0]) || !IsDigit(p[1]) || p[2] != ':' || !IsDigit(p[3])
		|| !IsDigit(p[4]) || p[5] != ':' || !IsDigit(p[6])
		|| !IsDigit(p[7])) {
		return false;
	}

	p += 8;
	if ((*p == '.' || *p == ',') && IsDigit(p[1])) {
		p++;
		while (IsDigit(*p))
			p++;
	}
	return true;
}


// "YYYY-MM-DD[T ]hh:mm:ss[.frac][Z|+hh:mm|+hhmm]", also with slashes.
bool
ParseISOTime(const char*& s, BString& time)
{
	const char* p = s;
	for (int32 i = 0; i < 4; i++) {
		if (!IsDigit(p[i]))
			return false;
	}
	char separator = p[4];
	if ((separator != '-' && separator != '/') || !IsDigit(p[5])
		|| !IsDigit(p[6]) || p[7] != separator || !IsDigit(p[8])
		|| !IsDigit(p[9]) || (p[10] != 'T' && p[10] != ' ')) {
		return false;
	}

	p += 11;
	if (!ParseClock(p))
		return false;

	if (*p == 'Z') {
		p++;
	} else if ((*p == '+' || *p == '-') && IsDigit(p[1]) && IsDigit(p[2])) {
		p += 3;
		if (*p == ':' && IsDigit(p[1]) && IsDigit(p[2]))
			p += 3;
		else if (IsDigit(p[0]) && IsDigit(p[1]))
			p += 2;
	}

	time.SetTo(s, p - s);
	s = p;
	return true;
}


// "Mmm dd hh:mm:ss", optionally with a year before the clock.
bool
ParseBSDTime(const char*& s, BString& time)
{
	bool isMonth = false;
	for (size_t i = 0; i < COUNT_OF(kMonths) && !isMonth; i++)
		isMonth = strncmp(s, kMonths[i], 3) == 0;
	if (!isMonth || s[3] != ' ')
		return false;

	const char* p = s + 4;
	if (*p == ' ')
		p++;
	if (!IsDigit(*p))
		return false;
	p++;
	if (IsDigit(*p))
		p++;
	if (*p != ' ')
		return false;
	p++;

	if (IsDigit(p[0]) && IsDigit(p[1]) && IsDigit(p[2]) && IsDigit(p[3])
		&& p[4] == ' ') {
		p += 5;
	}
	if (!ParseClock(p))
		return false;

	time.SetTo(s, p - s);
	s = p;
	return true;
}


// A time stamp in brackets, such as "[2024-01-05 10:00:00]" or the
// kernel's "[   12.345678]".
bool
ParseBracketTime(const char*& s, BString& time)
{
	if (*s != '[')
		return false;

	const char* p = s + 1;
	bool hasDigit = false;
	bool hasSeparator = false;
	while (*p != '\0' && *p != ']') {
		char c = *p;
		if (IsDigit(c))
			hasDigit = true;
		else if (c == ':' || c == '.')
			hasSeparator = true;
		else if (!IsAlpha(c) && c != ' ' && c != '-' && c != '/' && c != ','
			&& c != '+') {
			return false;
		}
		if (p - s > 40)
			return false;
		p++;
	}
	if (*p != ']' || !hasDigit || !hasSeparator)
		return false;

	time.SetTo(s + 1, p - s - 1);
	time.Trim();
	s = p + 1;
	return true;
}


// A level in front of a message: "ERROR", "[warn]", "<info>", "Debug:".
bool
ParseLevelToken(const char*& s, int32& severity)
{
	const char* p = s;
	char close = '\0';
	if (*p == '[')
		close = ']';
	else if (*p == '<')
		close = '>';
	else if (*p == '(')
		close = ')';
	if (close != '\0')
		p++;

	const char* word = p;
	while (IsAlpha(*p))
		p++;
	int32 level = LevelForWord(word, p - word, false);
	if (level == kSeverityNone)
		return false;

	if (close != '\0') {
		if (*p != close)
			return false;
		p++;
	}
	if (*p == ':')
		p++;
	if (*p != ' ' && *p != '\t' && *p != '\0')
		return false;
	SkipSpaces(p);

	severity = level;
	s = p;
	return true;
}


// The BSD tag: "name[pid]: " or "name: ".
bool
ParseTag(const char*& s, BString& source, BString& pid)
{
	const char* p = s;
	while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '[' && *p != ':')
		p++;
	if (p == s || p - s > 64)
		return false;

	const char* nameEnd = p;
	const char* pidStart = NULL;
	const char* pidEnd = NULL;
	if (*p == '[') {
		pidStart = ++p;
		while (*p != '\0' && *p != ']' && *p != ' ' && p - pidStart <= 20)
			p++;
		if (*p != ']' || p == pidStart)
			return false;
		pidEnd = p++;
	}
	if (*p != ':')
		return false;
	p++;
	if (*p != ' ' && *p != '\t' && *p != '\0')
		return false;
	SkipSpaces(p);

	source.SetTo(s, nameEnd - s);
	if (pidStart != NULL)
		pid.SetTo(pidStart, pidEnd - pidStart);
	s = p;
	return true;
}


// Haiku's syslog_daemon: "FACILITY['name'][[pid]]: ".
bool
ParseHaikuHeader(const char*& s, LogRecord& record)
{
	const char* p = s;
	while (isupper((unsigned char)*p) || IsDigit(*p))
		p++;

	bool isFacility = false;
	for (size_t i = 0; i < COUNT_OF(kFacilities) && !isFacility; i++) {
		isFacility = (size_t)(p - s) == strlen(kFacilities[i])
			&& strncmp(s, kFacilities[i], p - s) == 0;
	}
	if (!isFacility)
		return false;

	BString facility(s, p - s);
	BString source;
	BString pid;
	if (p[0] == ' ' && p[1] == '\'') {
		const char* nameStart = p + 2;
		const char* nameEnd = strchr(nameStart, '\'');
		if (nameEnd == NULL)
			return false;
		source.SetTo(nameStart, nameEnd - nameStart);
		p = nameEnd + 1;

		if (*p == '[') {
			const char* pidStart = ++p;
			while (IsDigit(*p))
				p++;
			if (*p != ']')
				return false;
			pid.SetTo(pidStart, p - pidStart);
			p++;
		}
	}
	if (*p != ':')
		return false;
	p++;
	SkipSpaces(p);

	record.fields[kFieldFacility] = facility;
	record.fields[kFieldSource] = source;
	record.fields[kFieldPID] = pid;
	s = p;
	return true;
}


// Most kernel messages start with the name of their driver or module.
bool
ParseComponent(const char*& s, BString& source)
{
	const char* p = s;
	while (IsWordChar(*p) || *p == '-' || *p == '.')
		p++;
	if (p == s || p - s > 40 || p[0] != ':' || p[1] != ' ')
		return false;

	source.SetTo(s, p - s);
	p += 2;
	SkipSpaces(p);
	s = p;
	return true;
}


// RFC 5424, starting after the priority:
// "VERSION TIMESTAMP HOST APP PROCID MSGID SD [MSG]".
bool
ParseRFC5424(const char*& s, LogRecord& record)
{
	const char* p = s;
	if (!IsDigit(*p))
		return false;
	while (IsDigit(*p))
		p++;
	if (*p != ' ')
		return false;
	p++;

	BString tokens[5];
	for (int32 i = 0; i < 5; i++) {
		const char* start = p;
		while (*p != '\0' && *p != ' ')
			p++;
		if (p == start)
			return false;
		tokens[i].SetTo(start, p - start);
		if (tokens[i] == "-")
			tokens[i].Truncate(0);
		if (*p == ' ')
			p++;
	}

	// Empty structured data; else it stays in front of the message.
	if (p[0] == '-' && (p[1] == ' ' || p[1] == '\0'))
		p += p[1] == ' ' ? 2 : 1;
	// The UTF-8 byte order mark that may start the message
	if (strncmp(p, "\xEF\xBB\xBF", 3) == 0)
		p += 3;

	record.fields[kFieldTime] = tokens[0];
	record.fields[kFieldHost] = tokens[1];
	record.fields[kFieldSource] = tokens[2];
	record.fields[kFieldPID] = tokens[3];
	s = p;
	return true;
}


}	// namespace


void
LogParser::Parse(const BString& line, int32 lineNumber, LogRecord& record)
{
	record.MakeEmpty();
	record.raw = line;
	record.lineNumber = lineNumber;
	record.fields[kFieldLine] << lineNumber;

	const char* s = line.String();
	int32 severity = kSeverityNone;

	int32 priority = ParsePriority(s);
	if (priority >= 0) {
		record.fields[kFieldFacility] = kFacilities[priority >> 3];
		severity = priority & 7;
	}

	if (priority < 0 || !ParseRFC5424(s, record)) {
		BString time;
		bool bsdTime = false;
		bool hasTime = ParseISOTime(s, time)
			|| (bsdTime = ParseBSDTime(s, time))
			|| ParseBracketTime(s, time);
		if (hasTime) {
			record.fields[kFieldTime] = time;
			SkipSpaces(s);
		}

		const char* p = s;
		int32 level;
		if (ParseHaikuHeader(s, record)) {
			if (!hasTime && ParseBracketTime(s, time)) {
				record.fields[kFieldTime] = time;
				SkipSpaces(s);
			}
			if (record.fields[kFieldSource].IsEmpty()
				&& record.fields[kFieldFacility] == "KERN") {
				ParseComponent(s, record.fields[kFieldSource]);
			}
		} else if (ParseLevelToken(p, level)) {
			if (severity == kSeverityNone)
				severity = level;
			// Without a time stamp in front, the word may as well be the
			// start of a sentence, so it stays in the message.
			if (hasTime)
				s = p;
		} else if (ParseTag(p, record.fields[kFieldSource],
				record.fields[kFieldPID])) {
			s = p;
		} else if (hasTime) {
			const char* hostEnd = s;
			while (*hostEnd != '\0' && *hostEnd != ' ' && *hostEnd != '\t')
				hostEnd++;
			p = hostEnd;
			SkipSpaces(p);
			if (ParseTag(p, record.fields[kFieldSource],
					record.fields[kFieldPID])
				|| (bsdTime && hostEnd > s)) {
				// BSD syslog always names the host; others only count as
				// such when a tag follows.
				record.fields[kFieldHost].SetTo(s, hostEnd - s);
				s = p;
			}
		}
	}

	BString& message = record.fields[kFieldMessage];
	message = s;
	message.ReplaceAll('\t', ' ');
	int32 length = message.Length();
	while (length > 0 && isspace((unsigned char)message[length - 1]))
		length--;
	message.Truncate(length);

	if (severity == kSeverityNone)
		severity = GuessSeverity(message.String());
	record.severity = severity;
	record.fields[kFieldSeverity] = SeverityName(severity);
}


int32
LogParser::GuessSeverity(const char* text)
{
	int32 result = kSeverityNone;
	const char* p = text;
	while (*p != '\0') {
		if (!IsAlpha(*p)) {
			p++;
			continue;
		}

		const char* start = p;
		while (IsAlpha(*p))
			p++;
		if ((start > text && IsWordChar(start[-1])) || IsWordChar(*p))
			continue;

		int32 level = LevelForWord(start, p - start, true);
		if (level != kSeverityNone
			&& (result == kSeverityNone || level < result)) {
			result = level;
		}
	}
	return result;
}
