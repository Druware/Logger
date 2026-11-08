/*
 * Copyright 2025-2026, Dru Satori @ Druware Software Designs.
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Logger
 * A parsed line of a log file.
 */
#ifndef LOG_RECORD_H
#define LOG_RECORD_H


#include <String.h>


// The columns of a record, in their default display order.
enum {
	kFieldLine = 0,
	kFieldTime,
	kFieldHost,
	kFieldFacility,
	kFieldSeverity,
	kFieldSource,
	kFieldPID,
	kFieldMessage,

	kFieldCount
};

// Pseudo fields that filters can test besides the columns.
enum {
	kFieldAny = -1,
		// any of the columns
	kFieldRaw = -2
		// the unparsed line
};

// syslog severities (RFC 5424); lower is more severe.
enum {
	kSeverityNone = -1,
	kSeverityEmergency = 0,
	kSeverityAlert,
	kSeverityCritical,
	kSeverityError,
	kSeverityWarning,
	kSeverityNotice,
	kSeverityInfo,
	kSeverityDebug,

	kSeverityCount
};


struct LogRecord {
								LogRecord();

			void				MakeEmpty();

			const BString&		Field(int32 field) const;

			BString				fields[kFieldCount];
			BString				raw;
			int32				lineNumber;
			int32				severity;
};


const char*	FieldName(int32 field);
const char*	SeverityName(int32 severity);


#endif	// LOG_RECORD_H
