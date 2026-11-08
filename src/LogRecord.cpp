/*
 * Logger
 * A parsed line of a log file.
 */


#include "LogRecord.h"


static const char* kFieldNames[kFieldCount] = {
	"Line",
	"Time",
	"Host",
	"Facility",
	"Severity",
	"Source",
	"PID",
	"Message"
};

static const char* kSeverityNames[kSeverityCount] = {
	"Emergency",
	"Alert",
	"Critical",
	"Error",
	"Warning",
	"Notice",
	"Info",
	"Debug"
};


LogRecord::LogRecord()
	:
	lineNumber(0),
	severity(kSeverityNone)
{
}


void
LogRecord::MakeEmpty()
{
	for (int32 i = 0; i < kFieldCount; i++)
		fields[i].Truncate(0);
	raw.Truncate(0);
	lineNumber = 0;
	severity = kSeverityNone;
}


const BString&
LogRecord::Field(int32 field) const
{
	if (field >= 0 && field < kFieldCount)
		return fields[field];
	return raw;
}


const char*
FieldName(int32 field)
{
	if (field == kFieldAny)
		return "Any column";
	if (field == kFieldRaw)
		return "Whole line";
	if (field >= 0 && field < kFieldCount)
		return kFieldNames[field];
	return "";
}


const char*
SeverityName(int32 severity)
{
	if (severity >= 0 && severity < kSeverityCount)
		return kSeverityNames[severity];
	return "";
}
