/*
 * Copyright 2025-2026, Dru Satori @ Druware Software Designs.
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Logger
 * A window that shows and follows one log file.
 */


#include "LogWindow.h"

#include <Application.h>
#include <Button.h>
#include <Clipboard.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageFilter.h>
#include <MessageRunner.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StatusBar.h>
#include <StringList.h>
#include <TextControl.h>
#include <TextView.h>

#include <algorithm>
#include <vector>

#include "FilterView.h"
#include "LogColumns.h"
#include "LoggerApp.h"
#include "LogListView.h"
#include "LogParser.h"


namespace {


enum {
	kMsgLoad				= 'load',
	kMsgReload				= 'rlod',
	kMsgPoll				= 'poll',
	kMsgRefilter			= 'rfil',
	kMsgSearchChanged		= 'srch',
	kMsgFind				= 'find',
	kMsgToggleFilters		= 'tgfl',
	kMsgToggleDetails		= 'tgdt',
	kMsgClearFilters		= 'clfl',
	kMsgJumpToEnd			= 'jend',
	kMsgSelectionChanged	= 'slch',
	kMsgRowInvoked			= 'rinv',
	kMsgContextMenu			= 'ctxm',
	kMsgAddFilter			= 'adfl',
	kMsgCopyValue			= 'cpvl'
};

const bigtime_t kPollInterval = 1000000;
const bigtime_t kRefilterDelay = 150000;

// Columns that are hidden when no line of the file fills them.
const uint32 kAutoHideColumns = (1 << kFieldTime) | (1 << kFieldHost)
	| (1 << kFieldFacility) | (1 << kFieldSource) | (1 << kFieldPID);


// Turns secondary clicks into the list into kMsgContextMenu messages.
class ContextMenuFilter : public BMessageFilter {
public:
	ContextMenuFilter(BHandler* target)
		:
		BMessageFilter(B_MOUSE_DOWN),
		fTarget(target)
	{
	}

	virtual filter_result Filter(BMessage* message, BHandler** _target)
	{
		int32 buttons = 0;
		int32 modifiers = 0;
		message->FindInt32("buttons", &buttons);
		message->FindInt32("modifiers", &modifiers);
		bool secondary = (buttons & B_SECONDARY_MOUSE_BUTTON) != 0
			|| ((buttons & B_PRIMARY_MOUSE_BUTTON) != 0
				&& (modifiers & B_CONTROL_KEY) != 0);
		BView* view = dynamic_cast<BView*>(*_target);
		if (!secondary || view == NULL)
			return B_DISPATCH_MESSAGE;

		BPoint where;
		BPoint screenWhere;
		if (message->FindPoint("screen_where", &screenWhere) != B_OK)
			return B_DISPATCH_MESSAGE;
		if (message->FindPoint("be:view_where", &where) != B_OK)
			where = view->ConvertFromScreen(screenWhere);

		BMessage contextMessage(kMsgContextMenu);
		contextMessage.AddPoint("where", where);
		contextMessage.AddPoint("screen_where", screenWhere);
		fTarget->Looper()->PostMessage(&contextMessage, fTarget);
		return B_SKIP_MESSAGE;
	}

private:
	BHandler*	fTarget;
};


BMessage*
FilterMessage(int32 field, int32 op, const char* value)
{
	BMessage* message = new BMessage(kMsgAddFilter);
	message->AddInt32("field", field);
	message->AddInt32("op", op);
	message->AddString("value", value);
	return message;
}


BString
Shortened(const BString& text)
{
	BString result(text);
	if (result.CountChars() > 40) {
		result.TruncateChars(40);
		result << B_UTF8_ELLIPSIS;
	}
	return result;
}


void
CopyText(const BString& text)
{
	if (!be_clipboard->Lock())
		return;

	be_clipboard->Clear();
	BMessage* data = be_clipboard->Data();
	if (data != NULL) {
		data->AddData("text/plain", B_MIME_TYPE, text.String(),
			text.Length());
		be_clipboard->Commit();
	}
	be_clipboard->Unlock();
}


}	// namespace


LogWindow::LogWindow(BRect frame, const entry_ref& ref)
	:
	BWindow(frame, ref.name, B_DOCUMENT_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	fRef(ref),
	fLineCount(0),
	fPollRunner(NULL),
	fRefilterRunner(NULL),
	fFollowing(true),
	fFollowValue(0),
	fFiltersVisible(true),
	fAutoHiddenColumns(0)
{
	BMenuBar* menuBar = _BuildMenuBar();

	fSearchControl = new BTextControl("search", "Search:", "", NULL);
	fSearchControl->SetModificationMessage(new BMessage(kMsgSearchChanged));

	fFiltersButton = new BButton("filters", "Filters",
		new BMessage(kMsgToggleFilters));
	fFiltersButton->SetBehavior(BButton::B_TOGGLE_BEHAVIOR);

	fFilterView = new FilterView(BMessenger(this));

	fListView = new LogListView("log");
	fListView->SetSelectionMode(B_MULTIPLE_SELECTION_LIST);
	fListView->SetPath(BPath(&ref).Path());
	fListView->SetSelectionMessage(new BMessage(kMsgSelectionChanged));
	fListView->SetInvocationMessage(new BMessage(kMsgRowInvoked));
	fListView->ScrollView()->AddFilter(new ContextMenuFilter(this));

	fDetailsView = new BTextView("details");
	fDetailsView->MakeEditable(false);
	fDetailsView->SetStylable(false);
	fDetailsView->SetWordWrap(true);
	fDetailsView->SetFontAndColor(be_fixed_font);
	fDetailsView->SetInsets(4, 4, 4, 4);
	BScrollView* detailsScrollView = new BScrollView("detailsScroll",
		fDetailsView, 0, false, true, B_NO_BORDER);

	fSplitView = new BSplitView(B_VERTICAL, 0);
	BLayoutBuilder::Split<>(fSplitView)
		.Add(fListView, 0.8f)
		.Add(detailsScrollView, 0.2f);
	fSplitView->SetCollapsible(0, false);

	// The bar shows how many of the lines pass the filters.
	fStatusBar = new BStatusBar("status");
	fStatusBar->SetBarHeight(8);
	// A status bar is at least as wide as its texts, which would make the
	// window grow with the path of the file.
	fStatusBar->SetExplicitMinSize(BSize(100, B_SIZE_UNSET));
	fStatusBar->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	// The resize knob of the document window covers the bottom right corner.
	float knobInset = be_control_look->GetScrollBarWidth(B_VERTICAL)
		+ BControlLook::ComposeSpacing(B_USE_SMALL_SPACING);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.AddGroup(B_HORIZONTAL, B_USE_HALF_ITEM_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING,
				B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING)
			.Add(fSearchControl)
			.Add(fFiltersButton)
		.End()
		.Add(fFilterView)
		.Add(fSplitView, 1.0f)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING, B_USE_SMALL_SPACING,
				knobInset, B_USE_SMALL_SPACING)
			.Add(fStatusBar, 1.0f)
		.End();

	fListView->SetTarget(this);
	_SetFiltersVisible(false);
	_SetDetailsVisible(true);
	fListView->MakeFocus(true);

	fTailer.SetTo(BPath(&ref).Path());
	_StartWatching();

	BMessage poll(kMsgPoll);
	fPollRunner = new BMessageRunner(BMessenger(this), &poll, kPollInterval);
	PostMessage(kMsgLoad);
}


LogWindow::~LogWindow()
{
	delete fPollRunner;
	delete fRefilterRunner;
}


void
LogWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgLoad:
			_Load();
			break;

		case kMsgReload:
			_Reload();
			break;

		case B_NODE_MONITOR:
			_ReadUpdates();
			break;

		case kMsgPoll:
			_ReadUpdates();
			_UpdateFollowing();
			_UpdateStatus();
			break;

		case kMsgSearchChanged:
		case kMsgFiltersChanged:
			_ScheduleRefilter();
			break;

		case kMsgRefilter:
			delete fRefilterRunner;
			fRefilterRunner = NULL;
			_Refilter();
			break;

		case kMsgFind:
			fSearchControl->MakeFocus(true);
			fSearchControl->TextView()->SelectAll();
			break;

		case kMsgToggleFilters:
			_SetFiltersVisible(!fFiltersVisible);
			if (fFiltersVisible && fFilterView->CountRules() == 0)
				fFilterView->AddRule(kFieldMessage, kOpContains, "");
			break;

		case kMsgToggleDetails:
			_SetDetailsVisible(fSplitView->IsItemCollapsed(1));
			break;

		case kMsgClearFilters:
			fFilterView->RemoveAllRules();
			fSearchControl->SetText("");
			_ScheduleRefilter();
			break;

		case kMsgJumpToEnd:
			_ScrollToEnd();
			_UpdateStatus();
			break;

		case kMsgSelectionChanged:
			_UpdateDetails();
			break;

		case kMsgRowInvoked:
			_SetDetailsVisible(true);
			break;

		case kMsgContextMenu:
		{
			BPoint where;
			BPoint screenWhere;
			if (message->FindPoint("where", &where) == B_OK
				&& message->FindPoint("screen_where", &screenWhere) == B_OK) {
				_ShowContextMenu(where, screenWhere);
			}
			break;
		}

		case kMsgAddFilter:
		{
			int32 field;
			int32 op;
			const char* value;
			if (message->FindInt32("field", &field) == B_OK
				&& message->FindInt32("op", &op) == B_OK
				&& message->FindString("value", &value) == B_OK) {
				fFilterView->AddRule(field, op, value, true);
				_SetFiltersVisible(true);
			}
			break;
		}

		case kMsgCopyValue:
		{
			const char* text;
			if (message->FindString("text", &text) == B_OK)
				CopyText(text);
			break;
		}

		case B_COPY:
			if (BTextView* textView = dynamic_cast<BTextView*>(CurrentFocus()))
				textView->Copy(be_clipboard);
			else
				_CopySelection();
			break;

		case B_SELECT_ALL:
			if (BTextView* textView = dynamic_cast<BTextView*>(CurrentFocus()))
				textView->SelectAll();
			else
				_SelectAll();
			break;

		case B_SIMPLE_DATA:
			if (message->HasRef("refs")) {
				BMessage refs(*message);
				refs.what = B_REFS_RECEIVED;
				be_app->PostMessage(&refs);
			}
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


bool
LogWindow::QuitRequested()
{
	stop_watching(this);
	be_app->PostMessage(kMsgWindowClosed);
	return true;
}


BMenuBar*
LogWindow::_BuildMenuBar()
{
	BMenuBar* menuBar = new BMenuBar("menu");

	BMenu* menu = new BMenu("File");
	BMenuItem* item = new BMenuItem("Open" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowOpenPanel), 'O');
	item->SetTarget(be_app);
	menu->AddItem(item);
	item = new BMenuItem("Open system log", new BMessage(kMsgOpenSystemLog),
		'O', B_SHIFT_KEY);
	item->SetTarget(be_app);
	menu->AddItem(item);
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Reload", new BMessage(kMsgReload), 'R'));
	menu->AddSeparatorItem();
	item = new BMenuItem("About Logger" B_UTF8_ELLIPSIS,
		new BMessage(B_ABOUT_REQUESTED));
	item->SetTarget(be_app);
	menu->AddItem(item);
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Close", new BMessage(B_QUIT_REQUESTED), 'W'));
	item = new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q');
	item->SetTarget(be_app);
	menu->AddItem(item);
	menuBar->AddItem(menu);

	menu = new BMenu("Edit");
	menu->AddItem(new BMenuItem("Copy", new BMessage(B_COPY), 'C'));
	menu->AddItem(new BMenuItem("Select all", new BMessage(B_SELECT_ALL),
		'A'));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Find", new BMessage(kMsgFind), 'F'));
	menuBar->AddItem(menu);

	menu = new BMenu("View");
	fFiltersItem = new BMenuItem("Show filters",
		new BMessage(kMsgToggleFilters), 'F', B_SHIFT_KEY);
	menu->AddItem(fFiltersItem);
	fDetailsItem = new BMenuItem("Show details",
		new BMessage(kMsgToggleDetails), 'D');
	menu->AddItem(fDetailsItem);
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Remove all filters",
		new BMessage(kMsgClearFilters)));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Jump to latest", new BMessage(kMsgJumpToEnd),
		'J'));
	menuBar->AddItem(menu);

	return menuBar;
}


void
LogWindow::_Load()
{
	_ReadUpdates();
	_HideEmptyColumns();

	// The list only knows its final size once the window is laid out.
	Layout(false);
	PostMessage(kMsgJumpToEnd);
}


void
LogWindow::_Reload()
{
	fListView->Clear();
	fRecords.clear();
	fLineCount = 0;
	fTailer.SetTo(BPath(&fRef).Path());
	_StartWatching();

	fFollowing = true;
	_ReadUpdates();
	_ScrollToEnd();
	_UpdateDetails();
	_UpdateStatus();
}


void
LogWindow::_ReadUpdates()
{
	BStringList lines;
	uint32 changes = fTailer.ReadLines(lines);
	if ((changes & LogTailer::kReplaced) != 0)
		_StartWatching();
	if ((changes & LogTailer::kTruncated) != 0) {
		fListView->Clear();
		fRecords.clear();
		fLineCount = 0;
	}
	if (lines.IsEmpty()) {
		if (changes != 0)
			_UpdateStatus();
		return;
	}

	// Whether to follow is decided before the new rows change the range.
	_UpdateFollowing();

	// Rows added one by one cost time proportional to the list each.
	BList rows;
	for (int32 i = 0; i < lines.CountStrings(); i++) {
		fLineCount++;
		BString line = lines.StringAt(i);
		if (line.IsEmpty())
			continue;

		fRecords.push_back(LogRecord());
		LogRecord& record = fRecords.back();
		LogParser::Parse(line, fLineCount, record);
		_ShowFilledColumns(record);
		if (_Matches(record))
			rows.AddItem(_CreateRow(fRecords.size() - 1));
	}
	if (!rows.IsEmpty())
		fListView->AddRows(&rows, -1);

	if (fFollowing)
		_ScrollToEnd();
	_UpdateStatus();
}


void
LogWindow::_StartWatching()
{
	stop_watching(this);

	if (fTailer.IsOpen())
		watch_node(&fTailer.NodeRef(), B_WATCH_STAT, this);

	// The directory tells when syslog is rotated and a new file appears.
	BEntry entry(fTailer.Path());
	BEntry parent;
	node_ref directory;
	if (entry.GetParent(&parent) == B_OK
		&& parent.GetNodeRef(&directory) == B_OK) {
		watch_node(&directory, B_WATCH_DIRECTORY, this);
	}
}


LogRow*
LogWindow::_CreateRow(int32 index) const
{
	return new LogRow(index, fRecords[index]);
}


bool
LogWindow::_Matches(const LogRecord& record) const
{
	return fSearch.Matches(record) && fFilters.Matches(record);
}


void
LogWindow::_ScheduleRefilter()
{
	delete fRefilterRunner;
	BMessage refilter(kMsgRefilter);
	fRefilterRunner = new BMessageRunner(BMessenger(this), &refilter,
		kRefilterDelay, 1);
}


void
LogWindow::_Refilter()
{
	fFilterView->GetFilters(fFilters);
	fSearch.MakeEmpty();
	fSearch.AddRule(kFieldAny, kOpContains, fSearchControl->Text(), false);
	fListView->SetFilters(fFilters, fSearch);

	_UpdateFollowing();

	// Keep the selection, or else the first visible line, in view.
	std::vector<int32> selected;
	for (BRow* row = fListView->CurrentSelection(); row != NULL;
			row = fListView->CurrentSelection(row)) {
		selected.push_back(static_cast<LogRow*>(row)->Index());
	}
	std::sort(selected.begin(), selected.end());

	int32 anchor = -1;
	if (!selected.empty())
		anchor = selected[0];
	else {
		BPoint top(0, fListView->ScrollView()->Bounds().top + 1);
		if (LogRow* row = dynamic_cast<LogRow*>(fListView->RowAt(top)))
			anchor = row->Index();
	}

	fListView->Clear();
	BList rows;
	std::vector<LogRow*> selectedRows;
	LogRow* anchorRow = NULL;
	for (int32 i = 0; i < (int32)fRecords.size(); i++) {
		if (!_Matches(fRecords[i]))
			continue;

		LogRow* row = _CreateRow(i);
		rows.AddItem(row);
		if (std::binary_search(selected.begin(), selected.end(), i))
			selectedRows.push_back(row);
		if (anchorRow == NULL && anchor >= 0 && i >= anchor)
			anchorRow = row;
	}
	if (!rows.IsEmpty())
		fListView->AddRows(&rows, -1);
	for (size_t i = 0; i < selectedRows.size(); i++)
		fListView->AddToSelection(selectedRows[i]);

	BRect rect;
	if (fFollowing)
		_ScrollToEnd();
	else if (anchorRow != NULL && fListView->GetRowRect(anchorRow, &rect))
		fListView->ScrollTo(BPoint(0, rect.top));

	_UpdateDetails();
	_UpdateStatus();
}


void
LogWindow::_HideEmptyColumns()
{
	uint32 filled = 0;
	for (size_t i = 0; i < fRecords.size(); i++) {
		for (int32 field = 0; field < kFieldCount; field++) {
			if (!fRecords[i].fields[field].IsEmpty())
				filled |= 1 << field;
		}
		if ((filled & kAutoHideColumns) == kAutoHideColumns)
			return;
	}

	fAutoHiddenColumns = kAutoHideColumns & ~filled;
	for (int32 i = 0; BColumn* column = fListView->ColumnAt(i); i++) {
		if ((fAutoHiddenColumns & (1 << column->LogicalFieldNum())) != 0)
			fListView->SetColumnVisible(column, false);
	}
}


void
LogWindow::_ShowFilledColumns(const LogRecord& record)
{
	if (fAutoHiddenColumns == 0)
		return;

	for (int32 i = 0; BColumn* column = fListView->ColumnAt(i); i++) {
		int32 field = column->LogicalFieldNum();
		if ((fAutoHiddenColumns & (1 << field)) != 0
			&& !record.fields[field].IsEmpty()) {
			fListView->SetColumnVisible(column, true);
			fAutoHiddenColumns &= ~(1 << field);
		}
	}
}


BScrollBar*
LogWindow::_VerticalScrollBar() const
{
	BScrollBar* scrollBar = fListView->ScrollView()->ScrollBar(B_VERTICAL);
	if (scrollBar == NULL) {
		scrollBar = dynamic_cast<BScrollBar*>(
			fListView->FindView("vertical_scroll_bar"));
	}
	return scrollBar;
}


// Following stops when the user scrolls away from the end, and resumes when
// they scroll back to it. Window resizes, which move the end without the
// user scrolling, do not stop it.
void
LogWindow::_UpdateFollowing()
{
	BScrollBar* scrollBar = _VerticalScrollBar();
	if (scrollBar == NULL) {
		fFollowing = true;
		return;
	}

	float value = scrollBar->Value();
	if (!fFollowing || value != fFollowValue) {
		float min;
		float max;
		scrollBar->GetRange(&min, &max);
		fFollowing = value >= max - 1;
	}
	fFollowValue = value;
}


void
LogWindow::_ScrollToEnd()
{
	int32 count = fListView->CountRows();
	if (count > 0)
		fListView->ScrollTo(fListView->RowAt(count - 1));

	fFollowing = true;
	if (BScrollBar* scrollBar = _VerticalScrollBar())
		fFollowValue = scrollBar->Value();
}


void
LogWindow::_SetFiltersVisible(bool visible)
{
	if (visible == fFiltersVisible)
		return;

	fFiltersVisible = visible;
	if (visible)
		fFilterView->Show();
	else
		fFilterView->Hide();
	fFiltersItem->SetMarked(visible);
	fFiltersButton->SetValue(visible ? B_CONTROL_ON : B_CONTROL_OFF);
}


void
LogWindow::_SetDetailsVisible(bool visible)
{
	fSplitView->SetItemCollapsed(1, !visible);
	fDetailsItem->SetMarked(visible);
}


void
LogWindow::_ShowContextMenu(BPoint where, BPoint screenWhere)
{
	LogRow* row = dynamic_cast<LogRow*>(fListView->RowAt(where));
	if (row == NULL)
		return;

	if (!row->IsSelected()) {
		fListView->DeselectAll();
		fListView->AddToSelection(row);
	}

	const LogRecord& record = fRecords[row->Index()];
	BColumn* column = fListView->ColumnAt(where);
	int32 field = column != NULL ? column->LogicalFieldNum() : kFieldMessage;
	const BString& value = record.fields[field];
	BString name(FieldName(field));
	BString shown = Shortened(value);

	BPopUpMenu* menu = new BPopUpMenu("context", false, false);
	BString label;
	label.SetToFormat("Show only lines whose %s is \"%s\"", name.String(),
		shown.String());
	menu->AddItem(new BMenuItem(label, FilterMessage(field, kOpIs, value)));
	label.SetToFormat("Hide lines whose %s is \"%s\"", name.String(),
		shown.String());
	menu->AddItem(new BMenuItem(label, FilterMessage(field, kOpIsNot, value)));

	if (field == kFieldSeverity && record.severity >= 0
		&& record.severity < kSeverityDebug) {
		label.SetToFormat("Show only %s and more severe", value.String());
		menu->AddItem(new BMenuItem(label, FilterMessage(kFieldSeverity,
			kOpGreaterThan, SeverityName(record.severity + 1))));
	}

	menu->AddSeparatorItem();
	BMessage* copyValue = new BMessage(kMsgCopyValue);
	copyValue->AddString("text", value);
	label.SetToFormat("Copy %s", name.String());
	menu->AddItem(new BMenuItem(label, copyValue));
	menu->AddItem(new BMenuItem("Copy lines", new BMessage(B_COPY)));

	if (fFilterView->CountRules() > 0 || fSearchControl->Text()[0] != '\0') {
		menu->AddSeparatorItem();
		menu->AddItem(new BMenuItem("Remove all filters",
			new BMessage(kMsgClearFilters)));
	}

	menu->SetTargetForItems(this);
	menu->SetAsyncAutoDestruct(true);
	menu->Go(screenWhere, true, false, true);
}


void
LogWindow::_CopySelection()
{
	BString text;
	for (BRow* row = fListView->CurrentSelection(); row != NULL;
			row = fListView->CurrentSelection(row)) {
		text << fRecords[static_cast<LogRow*>(row)->Index()].raw << '\n';
	}
	if (!text.IsEmpty())
		CopyText(text);
}


void
LogWindow::_SelectAll()
{
	// One selection message for all rows, not one per row
	fListView->SetSelectionMessage(NULL);
	for (int32 i = 0; BRow* row = fListView->RowAt(i); i++)
		fListView->AddToSelection(row);
	fListView->SetSelectionMessage(new BMessage(kMsgSelectionChanged));
	_UpdateDetails();
}


void
LogWindow::_UpdateDetails()
{
	LogRow* row = dynamic_cast<LogRow*>(fListView->CurrentSelection());
	if (row == NULL) {
		fDetailsView->SetText("");
		return;
	}

	const LogRecord& record = fRecords[row->Index()];
	BString text;
	for (int32 field = 0; field < kFieldCount; field++) {
		if (record.fields[field].IsEmpty())
			continue;
		BString line;
		line.SetToFormat("%-10s%s\n", FieldName(field),
			record.fields[field].String());
		text << line;
	}
	text << "\n" << record.raw;
	fDetailsView->SetText(text.String());
}


void
LogWindow::_UpdateStatus()
{
	int32 shown = fListView->CountRows();
	int32 total = (int32)fRecords.size();

	BString status;
	status.SetToFormat("%" B_PRId32 " of %" B_PRId32 " lines", shown,
		total);
	if (!fFilters.IsEmpty() || !fSearch.IsEmpty())
		status << " (filtered)";
	status << "  " B_UTF8_BULLET "  " << fTailer.Path();
	if (!fTailer.IsOpen())
		status << " (not found)";

	fStatusBar->SetMaxValue(total > 0 ? total : 1);
	fStatusBar->SetTo(shown, status.String(), fFollowing
		? "Following new lines" : "Paused: scroll to the end to follow");
}
