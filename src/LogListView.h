/*
 * Logger
 * The list of log lines, which can be dragged off as a replicant.
 */
#ifndef LOG_LIST_VIEW_H
#define LOG_LIST_VIEW_H


#include <Message.h>
#include <String.h>

#include <private/interface/ColumnListView.h>

#include "LogFilter.h"


class BDragger;
class BMessageRunner;
class BScrollBar;
class LogTailer;


// In a window, the list shows the rows the window adds. Its dragger makes a
// replicant that shows the same file with the window's columns and filters,
// and that follows the file on its own.
class LogListView : public BColumnListView {
public:
								LogListView(const char* name);
								LogListView(BMessage* archive);
	virtual						~LogListView();

	static	BArchivable*		Instantiate(BMessage* archive);
	virtual	status_t			Archive(BMessage* archive,
									bool deep = true) const;

	virtual	void				AttachedToWindow();
	virtual	void				DetachedFromWindow();
	virtual	void				MessageReceived(BMessage* message);

	// What a replicant made of the list shows.
			void				SetPath(const char* path);
			void				SetFilters(const FilterSet& filters,
									const FilterSet& search);

protected:
	virtual	void				DoLayout();

private:
			void				_Init();
			void				_AddColumns();

			void				_ReadUpdates();
			BScrollBar*			_VerticalScrollBar() const;
			bool				_IsAtEnd() const;
			void				_ScrollToEnd();

			BDragger*			fDragger;
			BString				fPath;
			BMessage			fFilterArchive;
			BMessage			fSearchArchive;

			// Used only by a replicant
			LogTailer*			fTailer;
			FilterSet			fFilters;
			FilterSet			fSearch;
			int32				fLineCount;
			BMessageRunner*		fPollRunner;
};


#endif	// LOG_LIST_VIEW_H
