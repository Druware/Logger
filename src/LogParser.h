/*
 * Copyright 2025-2026, Dru Satori @ Druware Software Designs.
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Logger
 * Splits log lines into records.
 */
#ifndef LOG_PARSER_H
#define LOG_PARSER_H


#include "LogRecord.h"


// Understands, in this order of preference:
//	- RFC 5424:	"<PRI>1 TIMESTAMP HOST APP PROCID MSGID SD MSG"
//	- Haiku:	"KERN: msg", "AUTH 'app'[pid]: msg", "DAEMON 'app': msg"
//	- BSD:		"[<PRI>]Mmm dd hh:mm:ss host tag[pid]: msg"
//	- ISO or bracketed time stamps followed by any of the above, by a
//	  level ("ERROR", "[warn]") or by free text.
// Anything else ends up whole in the message column.
class LogParser {
public:
	static	void				Parse(const BString& line, int32 lineNumber,
									LogRecord& record);

	// The most severe level word in text, or kSeverityNone.
	static	int32				GuessSeverity(const char* text);
};


#endif	// LOG_PARSER_H
