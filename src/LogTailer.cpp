/*
 * Logger
 * Reads the lines appended to a log file.
 */


#include "LogTailer.h"

#include <string.h>
#include <sys/stat.h>


static const size_t kChunkSize = 32 * 1024;


LogTailer::LogTailer()
	:
	fOffset(0)
{
}


status_t
LogTailer::SetTo(const char* path)
{
	Unset();

	status_t status = fPath.SetTo(path);
	if (status != B_OK)
		return status;
	return _Open();
}


void
LogTailer::Unset()
{
	fFile.Unset();
	fNodeRef = node_ref();
	fOffset = 0;
	fPending.Truncate(0);
}


bool
LogTailer::IsOpen() const
{
	return fFile.InitCheck() == B_OK;
}


uint32
LogTailer::ReadLines(BStringList& lines)
{
	int32 countBefore = lines.CountStrings();
	uint32 changes = 0;

	struct stat st;
	bool pathExists = stat(fPath.Path(), &st) == 0;
	if (!IsOpen()) {
		if (!pathExists || _Open() != B_OK)
			return 0;
		changes |= kReplaced;
	} else if (pathExists && (st.st_dev != fNodeRef.device
			|| st.st_ino != fNodeRef.node)) {
		// Another file took the path: finish the old one, then go on with
		// the new one. While the path is missing, the old file is kept.
		_ReadAvailable(lines);
		_FlushPending(lines);
		if (_Open() == B_OK)
			changes |= kReplaced;
	}

	off_t size;
	if (fFile.GetSize(&size) == B_OK && size < fOffset) {
		fOffset = 0;
		fPending.Truncate(0);
		lines.MakeEmpty();
		countBefore = 0;
		changes |= kTruncated;
	}

	// A last line without a line break is shown once the file has stopped
	// growing for a moment.
	if (_ReadAvailable(lines) == 0)
		_FlushPending(lines);

	if (lines.CountStrings() > countBefore)
		changes |= kAppended;
	return changes;
}


status_t
LogTailer::_Open()
{
	fOffset = 0;
	fPending.Truncate(0);

	status_t status = fFile.SetTo(fPath.Path(), B_READ_ONLY);
	if (status == B_OK)
		status = fFile.GetNodeRef(&fNodeRef);
	if (status != B_OK)
		fFile.Unset();
	return status;
}


off_t
LogTailer::_ReadAvailable(BStringList& lines)
{
	if (!IsOpen())
		return 0;

	char buffer[kChunkSize];
	off_t total = 0;
	while (true) {
		ssize_t bytesRead = fFile.ReadAt(fOffset, buffer, sizeof(buffer));
		if (bytesRead <= 0)
			break;
		fOffset += bytesRead;
		total += bytesRead;

		const char* start = buffer;
		const char* end = buffer + bytesRead;
		while (start < end) {
			const char* lineEnd
				= (const char*)memchr(start, '\n', end - start);
			if (lineEnd == NULL) {
				fPending.Append(start, end - start);
				break;
			}
			fPending.Append(start, lineEnd - start);
			if (fPending.EndsWith("\r"))
				fPending.Truncate(fPending.Length() - 1);
			lines.Add(fPending);
			fPending.Truncate(0);
			start = lineEnd + 1;
		}
	}
	return total;
}


void
LogTailer::_FlushPending(BStringList& lines)
{
	if (fPending.IsEmpty())
		return;

	if (fPending.EndsWith("\r"))
		fPending.Truncate(fPending.Length() - 1);
	lines.Add(fPending);
	fPending.Truncate(0);
}
