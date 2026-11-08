/*
 * Logger
 * Rules that select log records.
 */
#ifndef LOG_FILTER_H
#define LOG_FILTER_H


#include <regex.h>

#include <vector>

#include "LogRecord.h"


class BMessage;


enum {
	kOpContains = 0,
	kOpNotContains,
	kOpIs,
	kOpIsNot,
	kOpStartsWith,
	kOpEndsWith,
	kOpMatches,
	kOpNotMatches,
	kOpLessThan,
	kOpGreaterThan,

	kOpCount
};

const char*	OperatorName(int32 op);


class FilterRule {
public:
	// field is a column, kFieldAny or kFieldRaw. Less and greater compare
	// numbers as numbers, severities by how severe they are, and anything
	// else as text.
								FilterRule(int32 field, int32 op,
									const char* value, bool caseSensitive);
								~FilterRule();

			status_t			InitCheck() const { return fStatus; }
			bool				Matches(const LogRecord& record) const;

			int32				Field() const { return fField; }
			int32				Operator() const;
			const BString&		Value() const { return fValue; }
			bool				IsCaseSensitive() const
									{ return fCaseSensitive; }

private:
								FilterRule(const FilterRule&);
			FilterRule&			operator=(const FilterRule&);

			bool				_Test(const LogRecord& record,
									int32 field) const;
			bool				_Compare(const LogRecord& record,
									int32 field, int& result) const;

			int32				fField;
			int32				fOp;
				// the positive form of the operator
			bool				fNegate;
			BString				fValue;
			bool				fCaseSensitive;
			bool				fValueIsNumber;
			double				fNumber;
			int32				fSeverityRank;
			regex_t				fRegex;
			bool				fHasRegex;
			status_t			fStatus;
};


class FilterSet {
public:
								FilterSet();
								~FilterSet();

			void				MakeEmpty();

	// Whether a record must match all rules, or one of them.
			void				SetMatchAll(bool matchAll);

	// Rules whose value is empty are skipped unless they test for
	// equality. Fails, adding nothing, for an invalid regular expression.
			status_t			AddRule(int32 field, int32 op, const char* value,
									bool caseSensitive);

			bool				IsEmpty() const { return fRules.empty(); }
			bool				Matches(const LogRecord& record) const;

	// Stores the rules in archive, or replaces them by those stored there.
			status_t			Archive(BMessage* archive) const;
			status_t			Unarchive(const BMessage* archive);

private:
								FilterSet(const FilterSet&);
			FilterSet&			operator=(const FilterSet&);

			std::vector<FilterRule*> fRules;
			bool				fMatchAll;
};


#endif	// LOG_FILTER_H
