/*
 * Logger
 * A window that shows and follows one log file.
 */
#ifndef LOG_WINDOW_H
#define LOG_WINDOW_H


#include <Entry.h>
#include <Window.h>
#include <deque>

#include "LogFilter.h"
#include "LogRecord.h"
#include "LogTailer.h"


class BButton;
class BMenuBar;
class BMenuItem;
class BMessageRunner;
class BScrollBar;
class BSplitView;
class BStatusBar;
class BTextControl;
class BTextView;
class FilterView;
class LogListView;
class LogRow;


class LogWindow : public BWindow {
public:
								LogWindow(BRect frame, const entry_ref& ref);
	virtual						~LogWindow();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

			const entry_ref&	Ref() const { return fRef; }

private:
			BMenuBar*			_BuildMenuBar();

			void				_Load();
			void				_Reload();
			void				_ReadUpdates();
			void				_StartWatching();

			LogRow*				_CreateRow(int32 index) const;
			bool				_Matches(const LogRecord& record) const;
			void				_ScheduleRefilter();
			void				_Refilter();
			void				_HideEmptyColumns();
			void				_ShowFilledColumns(const LogRecord& record);

			BScrollBar*			_VerticalScrollBar() const;
			void				_UpdateFollowing();
			void				_ScrollToEnd();

			void				_SetFiltersVisible(bool visible);
			void				_SetDetailsVisible(bool visible);
			void				_ShowContextMenu(BPoint where,
									BPoint screenWhere);
			void				_CopySelection();
			void				_SelectAll();
			void				_UpdateDetails();
			void				_UpdateStatus();

			entry_ref			fRef;
			LogTailer			fTailer;
			std::deque<LogRecord> fRecords;
			int32				fLineCount;
			FilterSet			fFilters;
			FilterSet			fSearch;

			LogListView*		fListView;
			FilterView*			fFilterView;
			BTextControl*		fSearchControl;
			BButton*			fFiltersButton;
			BSplitView*			fSplitView;
			BTextView*			fDetailsView;
			BStatusBar*			fStatusBar;
			BMenuItem*			fFiltersItem;
			BMenuItem*			fDetailsItem;

			BMessageRunner*		fPollRunner;
			BMessageRunner*		fRefilterRunner;

			bool				fFollowing;
			float				fFollowValue;
			bool				fFiltersVisible;
			uint32				fAutoHiddenColumns;
};


#endif	// LOG_WINDOW_H
