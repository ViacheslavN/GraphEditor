#pragma once

// Redo log of a commit: the pages of a write transaction are written to "<database>-redo" and flushed, then
// copied into the database file; the log is deleted when the file is flushed.
//
//   header: uint32 magic, uint32 version, uint32 page size
//   page:   int64 address, uint32 crc32 of the data, data (page size)
//   commit: int64 -1, uint32 page count, uint32 crc32 of the page crcs
//
// The transaction is committed when the commit record is in the log. Open replays a complete log (a crash
// after it was written: the file may have a part of the pages) and deletes an incomplete one (a crash while
// it was written: the file wasn't touched).

#include "../DBCommon.h"
#include "TranStorageIO.h"

namespace bptreedb
{
	namespace db
	{
		class CRedoLog
		{
		public:
			// writes the private pages of a transaction and the commit record, flushes the log
			static void Write(const std::wstring& sPath, CTranStorageIO& pages);

			// a log left by a crash: a complete one is written into the storage (flushed), then the log is deleted.
			// Returns true if pages were replayed.
			static bool Recover(storage::IStorageIOPtr ptrStorage, const std::wstring& sPath);

			static void Delete(const std::wstring& sPath);

		private:
			static constexpr uint32_t Magic = 0x4C525042;     // "BPRL"
			static constexpr uint32_t Version = 1;
			static constexpr int64_t CommitMark = -1;
		};
	}
}
