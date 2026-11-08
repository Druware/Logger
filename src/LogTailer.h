/*
 * Logger
 * Reads the lines appended to a log file.
 */
#ifndef LOG_TAILER_H
#define LOG_TAILER_H


#include <File.h>
#include <Node.h>
#include <Path.h>
#include <String.h>
#include <StringList.h>


class LogTailer {
public:
	// What happened to the file since the last ReadLines().
	enum {
		kAppended	= 0x01,
		kTruncated	= 0x02,
			// the file got shorter: earlier lines are gone
		kReplaced	= 0x04
			// a new file took the path's place, as when syslog rotates
	};

								LogTailer();

			status_t			SetTo(const char* path);
			void				Unset();

			const char*			Path() const { return fPath.Path(); }
			const node_ref&		NodeRef() const { return fNodeRef; }
			bool				IsOpen() const;

	// Adds the complete lines written since the last call to lines and
	// returns a combination of the flags above.
			uint32				ReadLines(BStringList& lines);

private:
			status_t			_Open();
			off_t			_ReadAvailable(BStringList& lines);
			void				_FlushPending(BStringList& lines);

			BPath				fPath;
			BFile				fFile;
			node_ref			fNodeRef;
			off_t				fOffset;
			BString				fPending;
};


#endif	// LOG_TAILER_H
