/*
 * Copyright 2025-2026, Dru Satori @ Druware Software Designs.
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Logger
 * Rules that select log records.
 */


#include "LogFilter.h"

#include <ctype.h>
#include <stdlib.h>

#include <Message.h>


static const char* kOperatorNames[kOpCount] = {
	"contains",
	"does not contain",
	"is",
	"is not",
	"starts with",
	"ends with",
	"matches regex",
	"does not match regex",
	"is less than",
	"is greater than"
};


static bool
ParseNumber(const char* text, double& number)
{
	while (isspace((unsigned char)*text))
		text++;
	if (*text == '\0')
		return false;

	char* end;
	number = strtod(text, &end);
	if (end == text)
		return false;
	while (isspace((unsigned char)*end))
		end++;
	return *end == '\0';
}


// Ranks severities so that more severe is greater; unknown is lowest.
static int32
SeverityRank(int32 severity)
{
	if (severity < 0 || severity >= kSeverityCount)
		return 0;
	return kSeverityCount - severity;
}


const char*
OperatorName(int32 op)
{
	if (op >= 0 && op < kOpCount)
		return kOperatorNames[op];
	return "";
}


// #pragma mark - FilterRule


FilterRule::FilterRule(int32 field, int32 op, const char* value,
	bool caseSensitive)
	:
	fField(field),
	fOp(op),
	fNegate(false),
	fValue(value),
	fCaseSensitive(caseSensitive),
	fValueIsNumber(false),
	fNumber(0),
	fSeverityRank(-1),
	fHasRegex(false),
	fStatus(B_OK)
{
	switch (op) {
		case kOpNotContains:
			fOp = kOpContains;
			fNegate = true;
			break;
		case kOpIsNot:
			fOp = kOpIs;
			fNegate = true;
			break;
		case kOpNotMatches:
			fOp = kOpMatches;
			fNegate = true;
			break;
	}

	if (fOp == kOpMatches) {
		int flags = REG_EXTENDED | REG_NOSUB;
		if (!caseSensitive)
			flags |= REG_ICASE;
		if (regcomp(&fRegex, fValue.String(), flags) == 0)
			fHasRegex = true;
		else
			fStatus = B_BAD_VALUE;
	} else if (fOp == kOpLessThan || fOp == kOpGreaterThan) {
		fValueIsNumber = ParseNumber(fValue.String(), fNumber);
		for (int32 i = 0; i < kSeverityCount; i++) {
			if (fValue.ICompare(SeverityName(i)) == 0)
				fSeverityRank = SeverityRank(i);
		}
	}
}


FilterRule::~FilterRule()
{
	if (fHasRegex)
		regfree(&fRegex);
}


bool
FilterRule::Matches(const LogRecord& record) const
{
	if (fStatus != B_OK)
		return true;

	bool result = false;
	if (fField == kFieldAny) {
		for (int32 i = 0; i < kFieldCount && !result; i++)
			result = _Test(record, i);
	} else
		result = _Test(record, fField);
	return result != fNegate;
}


// The operator as it was given, negated or not.
int32
FilterRule::Operator() const
{
	if (!fNegate)
		return fOp;

	switch (fOp) {
		case kOpContains:
			return kOpNotContains;
		case kOpIs:
			return kOpIsNot;
		case kOpMatches:
			return kOpNotMatches;
	}
	return fOp;
}


bool
FilterRule::_Test(const LogRecord& record, int32 field) const
{
	const BString& text = record.Field(field);
	switch (fOp) {
		case kOpContains:
			return (fCaseSensitive
				? text.FindFirst(fValue) : text.IFindFirst(fValue)) >= 0;
		case kOpIs:
			return (fCaseSensitive
				? text.Compare(fValue) : text.ICompare(fValue)) == 0;
		case kOpStartsWith:
			return fCaseSensitive
				? text.StartsWith(fValue) : text.IStartsWith(fValue);
		case kOpEndsWith:
			return fCaseSensitive
				? text.EndsWith(fValue) : text.IEndsWith(fValue);
		case kOpMatches:
			return regexec(&fRegex, text.String(), 0, NULL, 0) == 0;
		case kOpLessThan:
		case kOpGreaterThan:
		{
			int result;
			if (!_Compare(record, field, result))
				return false;
			return fOp == kOpLessThan ? result < 0 : result > 0;
		}
	}
	return false;
}


// Fails when the value is a number or a severity and the field has none.
bool
FilterRule::_Compare(const LogRecord& record, int32 field, int& result) const
{
	if (field == kFieldSeverity && fSeverityRank >= 0) {
		if (record.severity == kSeverityNone)
			return false;
		result = SeverityRank(record.severity) - fSeverityRank;
		return true;
	}

	const BString& text = record.Field(field);
	if (fValueIsNumber) {
		double number;
		if (!ParseNumber(text.String(), number))
			return false;
		result = number < fNumber ? -1 : (number > fNumber ? 1 : 0);
		return true;
	}

	result = fCaseSensitive ? text.Compare(fValue) : text.ICompare(fValue);
	return true;
}


// #pragma mark - FilterSet


FilterSet::FilterSet()
	:
	fMatchAll(true)
{
}


FilterSet::~FilterSet()
{
	MakeEmpty();
}


void
FilterSet::MakeEmpty()
{
	for (size_t i = 0; i < fRules.size(); i++)
		delete fRules[i];
	fRules.clear();
}


void
FilterSet::SetMatchAll(bool matchAll)
{
	fMatchAll = matchAll;
}


status_t
FilterSet::AddRule(int32 field, int32 op, const char* value,
	bool caseSensitive)
{
	if ((value == NULL || value[0] == '\0') && op != kOpIs && op != kOpIsNot)
		return B_OK;

	FilterRule* rule = new FilterRule(field, op, value, caseSensitive);
	status_t status = rule->InitCheck();
	if (status != B_OK) {
		delete rule;
		return status;
	}
	fRules.push_back(rule);
	return B_OK;
}


bool
FilterSet::Matches(const LogRecord& record) const
{
	if (fRules.empty())
		return true;

	for (size_t i = 0; i < fRules.size(); i++) {
		bool matches = fRules[i]->Matches(record);
		if (matches != fMatchAll)
			return matches;
	}
	return fMatchAll;
}


status_t
FilterSet::Archive(BMessage* archive) const
{
	status_t status = archive->AddBool("match_all", fMatchAll);
	for (size_t i = 0; i < fRules.size() && status == B_OK; i++) {
		const FilterRule* rule = fRules[i];
		status = archive->AddInt32("field", rule->Field());
		if (status == B_OK)
			status = archive->AddInt32("op", rule->Operator());
		if (status == B_OK)
			status = archive->AddString("value", rule->Value());
		if (status == B_OK) {
			status = archive->AddBool("case_sensitive",
				rule->IsCaseSensitive());
		}
	}
	return status;
}


status_t
FilterSet::Unarchive(const BMessage* archive)
{
	MakeEmpty();
	SetMatchAll(archive->GetBool("match_all", true));

	int32 field;
	for (int32 i = 0; archive->FindInt32("field", i, &field) == B_OK; i++) {
		int32 op;
		const char* value;
		bool caseSensitive;
		if (archive->FindInt32("op", i, &op) != B_OK
			|| archive->FindString("value", i, &value) != B_OK
			|| archive->FindBool("case_sensitive", i, &caseSensitive)
				!= B_OK) {
			return B_BAD_DATA;
		}
		AddRule(field, op, value, caseSensitive);
	}
	return B_OK;
}
