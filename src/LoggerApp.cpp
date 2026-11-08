/*
 * Copyright 2025-2026, Dru Satori @ Druware Software Designs.
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 * Logger
 * The application: opens a window for each log file.
 */


#include "LoggerApp.h"

#include <Alert.h>
#include <private/interface/AboutWindow.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Path.h>

#include <string.h>

#include "LogWindow.h"


static const char* kLicenseText =
	"Permission is hereby granted, free of charge, to any person obtaining "
	"a copy of this software and associated documentation files (the "
	"\"Software\"), to deal in the Software without restriction, including "
	"without limitation the rights to use, copy, modify, merge, publish, "
	"distribute, sublicense, and/or sell copies of the Software, and to "
	"permit persons to whom the Software is furnished to do so, subject to "
	"the following conditions:\n\n"
	"The above copyright notice and this permission notice shall be "
	"included in all copies or substantial portions of the Software.\n\n"
	"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, "
	"EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF "
	"MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. "
	"IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY "
	"CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, "
	"TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE "
	"SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.";


static void
ShowError(const char* text, const char* name, status_t status)
{
	BString message;
	message.SetToFormat(text, name, strerror(status));
	BAlert* alert = new BAlert("Logger", message.String(), "OK", NULL, NULL,
		B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
	alert->Go(NULL);
}


LoggerApp::LoggerApp()
	:
	BApplication(kSignature),
	fOpenPanel(NULL),
	fWindowCount(0),
	fWindowsOpened(0)
{
}


LoggerApp::~LoggerApp()
{
	delete fOpenPanel;
}


void
LoggerApp::ReadyToRun()
{
	if (fWindowCount == 0)
		_OpenSystemLog();
	if (fWindowCount == 0)
		_ShowOpenPanel();
}


void
LoggerApp::ArgvReceived(int32 argc, char** argv)
{
	const char* cwd = NULL;
	if (CurrentMessage() != NULL)
		CurrentMessage()->FindString("cwd", &cwd);

	for (int32 i = 1; i < argc; i++) {
		BPath path;
		if (argv[i][0] != '/' && cwd != NULL)
			path.SetTo(cwd, argv[i]);
		else
			path.SetTo(argv[i]);

		entry_ref ref;
		status_t status = get_ref_for_path(path.Path(), &ref);
		if (status != B_OK) {
			ShowError("Could not open \"%s\":\n%s", argv[i], status);
			continue;
		}
		_Open(ref);
	}
}


void
LoggerApp::RefsReceived(BMessage* message)
{
	entry_ref ref;
	for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
		_Open(ref);
}


void
LoggerApp::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgShowOpenPanel:
			_ShowOpenPanel();
			break;

		case kMsgOpenSystemLog:
			_OpenSystemLog();
			break;

		case kMsgWindowClosed:
			if (--fWindowCount <= 0)
				PostMessage(B_QUIT_REQUESTED);
			break;

		case B_CANCEL:
			// The open panel closed; without a window there is nothing to do.
			if (fWindowCount <= 0)
				PostMessage(B_QUIT_REQUESTED);
			break;

		default:
			BApplication::MessageReceived(message);
	}
}


void
LoggerApp::AboutRequested()
{
	BAboutWindow* window = new BAboutWindow("Logger", kSignature);
	window->AddDescription("Views and follows the system log and other log "
		"files, and filters them by any column.");
	window->AddText(kCopyright);

	BString license(kLicenseName);
	license << "\n\n" << kCopyright << "\n\n" << kLicenseText;
	window->AddExtraInfo(license.String());
	window->Show();
}


void
LoggerApp::_Open(const entry_ref& unresolvedRef)
{
	BEntry entry(&unresolvedRef, true);
	entry_ref ref;
	status_t status = entry.GetRef(&ref);
	if (status == B_OK && entry.IsDirectory())
		status = B_IS_A_DIRECTORY;
	if (status == B_OK) {
		BFile file(&ref, B_READ_ONLY);
		status = file.InitCheck();
	}
	if (status != B_OK) {
		ShowError("Could not open \"%s\":\n%s", unresolvedRef.name, status);
		return;
	}

	for (int32 i = 0; BWindow* window = WindowAt(i); i++) {
		LogWindow* logWindow = dynamic_cast<LogWindow*>(window);
		if (logWindow != NULL && logWindow->Ref() == ref) {
			logWindow->Activate();
			return;
		}
	}

	float offset = (fWindowsOpened % 10) * 24.0f;
	BRect frame(80, 60, 1180, 760);
	frame.OffsetBy(offset, offset);

	LogWindow* window = new LogWindow(frame, ref);
	window->Show();
	fWindowCount++;
	fWindowsOpened++;
}


void
LoggerApp::_OpenSystemLog()
{
	BPath path;
	status_t status = find_directory(B_SYSTEM_LOG_DIRECTORY, &path);
	if (status == B_OK)
		status = path.Append("syslog");

	entry_ref ref;
	if (status == B_OK)
		status = get_ref_for_path(path.Path(), &ref);
	if (status != B_OK) {
		ShowError("Could not open \"%s\":\n%s", "syslog", status);
		return;
	}
	_Open(ref);
}


void
LoggerApp::_ShowOpenPanel()
{
	if (fOpenPanel == NULL) {
		fOpenPanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL, B_FILE_NODE,
			true);

		BPath path;
		if (find_directory(B_SYSTEM_LOG_DIRECTORY, &path) == B_OK)
			fOpenPanel->SetPanelDirectory(path.Path());
	}
	fOpenPanel->Show();
}
