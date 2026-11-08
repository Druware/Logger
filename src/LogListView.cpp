/*
 * Copyright 2025-2026, Dru Satori @ Druware Software Designs.
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Logger
 * The list of log lines, which can be dragged off as a replicant.
 */


#include "LogListView.h"

#include <Alert.h>
#include <Dragger.h>
#include <MessageRunner.h>
#include <ScrollBar.h>
#include <StringList.h>

#include "LogColumns.h"
#include "LoggerApp.h"
#include "LogParser.h"
#include "LogRecord.h"
#include "LogTailer.h"


namespace {


const uint32 kMsgPoll = 'lpol';
const bigtime_t kPollInterval = 1000000;

// A replicant keeps only the latest lines.
const int32 kMaxReplicantRows = 1000;


BRect
ArchivedFrame(const BMessage* archive)
{
	BRect frame;
	if (archive->FindRect("_frame", &frame) != B_OK)
		frame.Set(0, 0, 599, 299);
	return frame;
}


}	// namespace


LogListView::LogListView(const char* name)
	:
	BColumnListView(name, 0, B_NO_BORDER, true),
	fDragger(NULL),
	fTailer(NULL),
	fLineCount(0),
	fPollRunner(NULL)
{
	_Init();
}


LogListView::LogListView(BMessage* archive)
	:
	BColumnListView(ArchivedFrame(archive), "log", B_FOLLOW_NONE, 0,
		B_NO_BORDER, true),
	fDragger(NULL),
	fTailer(new LogTailer),
	fLineCount(0),
	fPollRunner(NULL)
{
	_Init();

	BMessage columns;
	if (archive->FindMessage("columns", &columns) == B_OK)
		LoadState(&columns);
	if (archive->FindMessage("filters", &fFilterArchive) == B_OK)
		fFilters.Unarchive(&fFilterArchive);
	if (archive->FindMessage("search", &fSearchArchive) == B_OK)
		fSearch.Unarchive(&fSearchArchive);

	archive->FindString("path", &fPath);
	fTailer->SetTo(fPath.String());
}


LogListView::~LogListView()
{
	delete fPollRunner;
	delete fTailer;
}


BArchivable*
LogListView::Instantiate(BMessage* archive)
{
	if (!validate_instantiation(archive, "LogListView"))
		return NULL;
	return new LogListView(archive);
}


status_t
LogListView::Archive(BMessage* archive, bool deep) const
{
	// The constructor makes the children anew.
	status_t status = BColumnListView::Archive(archive, false);
	if (status == B_OK)
		status = archive->AddString("add_on", kSignature);
	if (status == B_OK)
		status = archive->AddString("path", fPath);

	BMessage columns;
	const_cast<LogListView*>(this)->SaveState(&columns);
	if (status == B_OK)
		status = archive->AddMessage("columns", &columns);
	if (status == B_OK)
		status = archive->AddMessage("filters", &fFilterArchive);
	if (status == B_OK)
		status = archive->AddMessage("search", &fSearchArchive);
	return status;
}


void
LogListView::AttachedToWindow()
{
	BColumnListView::AttachedToWindow();

	if (fTailer == NULL)
		return;

	_ReadUpdates();
	_ScrollToEnd();

	BMessage poll(kMsgPoll);
	fPollRunner = new BMessageRunner(BMessenger(this), &poll, kPollInterval);
}


void
LogListView::DetachedFromWindow()
{
	delete fPollRunner;
	fPollRunner = NULL;

	BColumnListView::DetachedFromWindow();
}


void
LogListView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgPoll:
			if (fTailer != NULL)
				_ReadUpdates();
			break;

		case B_ABOUT_REQUESTED:
		{
			BString text;
			text.SetToFormat("Logger\n\nShows and follows %s.\n\n%s\n"
				"Distributed under the terms of the %s.", fPath.String(),
				kCopyright, kLicenseName);
			BAlert* alert = new BAlert("About", text.String(), "OK");
			alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
			alert->Go(NULL);
			break;
		}

		default:
			BColumnListView::MessageReceived(message);
	}
}


void
LogListView::SetPath(const char* path)
{
	fPath = path;
}


void
LogListView::SetFilters(const FilterSet& filters, const FilterSet& search)
{
	fFilterArchive.MakeEmpty();
	filters.Archive(&fFilterArchive);
	fSearchArchive.MakeEmpty();
	search.Archive(&fSearchArchive);
}


void
LogListView::DoLayout()
{
	BColumnListView::DoLayout();

	// The dragger sits in the corner between the scroll bars.
	BRect bounds = Bounds();
	fDragger->MoveTo(bounds.right - fDragger->Bounds().Width(),
		bounds.bottom - fDragger->Bounds().Height());
}


void
LogListView::_Init()
{
	SetColumnFlags((column_flags)(B_ALLOW_COLUMN_MOVE
		| B_ALLOW_COLUMN_RESIZE | B_ALLOW_COLUMN_POPUP));
	_AddColumns();

	BRect rect = Bounds();
	rect.left = rect.right - 7;
	rect.top = rect.bottom - 7;
	fDragger = new BDragger(rect, this, B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);
	AddChild(fDragger);
}


void
LogListView::_AddColumns()
{
	struct ColumnInfo {
		int32	field;
		float	width;
		bool	numeric;
	};
	const ColumnInfo columns[] = {
		{ kFieldLine, 60, true },
		{ kFieldTime, 160, false },
		{ kFieldHost, 100, false },
		{ kFieldFacility, 80, false },
		{ kFieldSeverity, 80, false },
		{ kFieldSource, 130, false },
		{ kFieldPID, 60, true },
		{ kFieldMessage, 800, false }
	};

	for (size_t i = 0; i < sizeof(columns) / sizeof(columns[0]); i++) {
		const ColumnInfo& info = columns[i];
		AddColumn(new LogColumn(FieldName(info.field), info.width, 30,
				info.field == kFieldMessage ? 10000 : 1000, info.numeric,
				info.numeric ? B_ALIGN_RIGHT : B_ALIGN_LEFT),
			info.field);
	}
}


void
LogListView::_ReadUpdates()
{
	BStringList lines;
	uint32 changes = fTailer->ReadLines(lines);
	if ((changes & LogTailer::kTruncated) != 0) {
		Clear();
		fLineCount = 0;
	}
	if (lines.IsEmpty())
		return;

	bool atEnd = _IsAtEnd();

	BList rows;
	for (int32 i = 0; i < lines.CountStrings(); i++) {
		fLineCount++;
		BString line = lines.StringAt(i);
		if (line.IsEmpty())
			continue;

		LogRecord record;
		LogParser::Parse(line, fLineCount, record);
		if (fSearch.Matches(record) && fFilters.Matches(record))
			rows.AddItem(new LogRow(fLineCount, record));
	}

	int32 excess = rows.CountItems() - kMaxReplicantRows;
	if (excess > 0) {
		for (int32 i = 0; i < excess; i++)
			delete static_cast<LogRow*>(rows.ItemAt(i));
		rows.RemoveItems(0, excess);
	}

	excess = CountRows() + rows.CountItems() - kMaxReplicantRows;
	if (excess > 0) {
		BList oldRows;
		for (int32 i = 0; i < excess; i++)
			oldRows.AddItem(RowAt(i));
		RemoveRows(&oldRows);
		for (int32 i = 0; i < excess; i++)
			delete static_cast<LogRow*>(oldRows.ItemAt(i));
	}

	if (!rows.IsEmpty())
		AddRows(&rows, -1);
	if (atEnd)
		_ScrollToEnd();
}


BScrollBar*
LogListView::_VerticalScrollBar() const
{
	BScrollBar* scrollBar = ScrollView()->ScrollBar(B_VERTICAL);
	if (scrollBar == NULL)
		scrollBar = dynamic_cast<BScrollBar*>(FindView("vertical_scroll_bar"));
	return scrollBar;
}


bool
LogListView::_IsAtEnd() const
{
	BScrollBar* scrollBar = _VerticalScrollBar();
	if (scrollBar == NULL)
		return true;

	float min;
	float max;
	scrollBar->GetRange(&min, &max);
	return scrollBar->Value() >= max - 1;
}


void
LogListView::_ScrollToEnd()
{
	int32 count = CountRows();
	if (count > 0)
		ScrollTo(RowAt(count - 1));
}
