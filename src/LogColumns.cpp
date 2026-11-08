/*
 * Logger
 * Columns, fields and rows of the log list.
 */


#include "LogColumns.h"

#include <stdlib.h>

#include "LogRecord.h"


static bool
SeverityColor(int32 severity, rgb_color text, rgb_color& color)
{
	// Light text means a dark background.
	bool dark = text.IsLight();
	switch (severity) {
		case kSeverityEmergency:
		case kSeverityAlert:
		case kSeverityCritical:
			color = dark ? make_color(255, 90, 90) : make_color(200, 0, 0);
			return true;
		case kSeverityError:
			color = dark ? make_color(255, 125, 115) : make_color(175, 20, 20);
			return true;
		case kSeverityWarning:
			color = dark ? make_color(255, 195, 90) : make_color(160, 95, 0);
			return true;
		case kSeverityDebug:
			color = tint_color(text, dark ? B_DARKEN_2_TINT : B_LIGHTEN_1_TINT);
			return true;
	}
	return false;
}


// #pragma mark - LogField


LogField::LogField(const char* string, int32 severity)
	:
	BStringField(string),
	fSeverity(severity)
{
}


// #pragma mark - LogColumn


LogColumn::LogColumn(const char* title, float width, float minWidth,
	float maxWidth, bool numeric, alignment align)
	:
	BStringColumn(title, width, minWidth, maxWidth, B_TRUNCATE_END, align),
	fNumeric(numeric)
{
}


void
LogColumn::DrawField(BField* _field, BRect rect, BView* parent)
{
	LogField* field = static_cast<LogField*>(_field);

	rgb_color text = parent->HighColor();
	rgb_color color;
	if (!SeverityColor(field->Severity(), text, color)) {
		BStringColumn::DrawField(field, rect, parent);
		return;
	}

	parent->SetHighColor(color);
	BStringColumn::DrawField(field, rect, parent);
	parent->SetHighColor(text);
}


int
LogColumn::CompareFields(BField* field1, BField* field2)
{
	if (!fNumeric)
		return BStringColumn::CompareFields(field1, field2);

	const char* string1 = static_cast<LogField*>(field1)->String();
	const char* string2 = static_cast<LogField*>(field2)->String();
	if (string1[0] == '\0' || string2[0] == '\0')
		return (string1[0] != '\0') - (string2[0] != '\0');

	long long number1 = strtoll(string1, NULL, 10);
	long long number2 = strtoll(string2, NULL, 10);
	return number1 < number2 ? -1 : (number1 > number2 ? 1 : 0);
}


bool
LogColumn::AcceptsField(const BField* field) const
{
	return dynamic_cast<const LogField*>(field) != NULL;
}


// #pragma mark - LogRow


LogRow::LogRow(int32 index)
	:
	fIndex(index)
{
}


LogRow::LogRow(int32 index, const LogRecord& record)
	:
	fIndex(index)
{
	for (int32 field = 0; field < kFieldCount; field++) {
		SetField(new LogField(record.fields[field].String(),
			record.severity), field);
	}
}
