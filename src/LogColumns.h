/*
 * Logger
 * Columns, fields and rows of the log list.
 */
#ifndef LOG_COLUMNS_H
#define LOG_COLUMNS_H


#include <private/interface/ColumnListView.h>
#include <private/interface/ColumnTypes.h>


struct LogRecord;


// A text that knows the severity of its record, to color it.
class LogField : public BStringField {
public:
								LogField(const char* string, int32 severity);

			int32				Severity() const { return fSeverity; }

private:
			int32				fSeverity;
};


class LogColumn : public BStringColumn {
public:
								LogColumn(const char* title, float width,
									float minWidth, float maxWidth,
									bool numeric,
									alignment align = B_ALIGN_LEFT);

	virtual	void				DrawField(BField* field, BRect rect,
									BView* parent);
	virtual	int					CompareFields(BField* field1,
									BField* field2);
	virtual	bool				AcceptsField(const BField* field) const;

private:
			bool				fNumeric;
};


// A row shows the record at an index of the window's record list.
class LogRow : public BRow {
public:
								LogRow(int32 index);
								LogRow(int32 index, const LogRecord& record);

			int32				Index() const { return fIndex; }

private:
			int32				fIndex;
};


#endif	// LOG_COLUMNS_H
