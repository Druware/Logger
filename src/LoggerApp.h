/*
 * Logger
 * The application: opens a window for each log file.
 */
#ifndef LOGGER_APP_H
#define LOGGER_APP_H


#include <Application.h>


class BFilePanel;
struct entry_ref;


// Also names the add-on that replicants of the log list are loaded from.
const char* const kSignature = "application/x-vnd.Logger";

const uint32 kMsgShowOpenPanel = 'opnp';
const uint32 kMsgOpenSystemLog = 'osys';
const uint32 kMsgWindowClosed = 'wcls';


class LoggerApp : public BApplication {
public:
								LoggerApp();
	virtual						~LoggerApp();

	virtual	void				ReadyToRun();
	virtual	void				ArgvReceived(int32 argc, char** argv);
	virtual	void				RefsReceived(BMessage* message);
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				AboutRequested();

private:
			void				_Open(const entry_ref& ref);
			void				_OpenSystemLog();
			void				_ShowOpenPanel();

			BFilePanel*			fOpenPanel;
			int32				fWindowCount;
			int32				fWindowsOpened;
};


#endif	// LOGGER_APP_H
