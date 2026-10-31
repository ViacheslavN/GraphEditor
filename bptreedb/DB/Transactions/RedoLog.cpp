#include "RedoLog.h"
#include "../../StorageLib/FileStorage.h"
#include "../../UtilsLib/CRC.h"
#include "../../../CommonLib/filesystem/filesystem.h"

namespace bptreedb
{
	namespace db
	{
		namespace
		{
			void WriteAll(CommonLib::file::TFilePtr& ptrFile, const void* pData, size_t nSize)
			{
				if (ptrFile->Write((const byte_t*)pData, nSize) != (std::streamsize)nSize)
					throw CommonLib::CExcBase("failed to write {0} bytes", (uint64_t)nSize);
			}

			bool ReadAll(CommonLib::file::TFilePtr& ptrFile, void* pData, size_t nSize)
			{
				return ptrFile->Read((byte_t*)pData, nSize) == (std::streamsize)nSize;
			}
		}

		void CRedoLog::Write(const std::wstring& sPath, CTranStorageIO& pages)
		{
			try
			{
				CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFile(sPath.c_str(), CommonLib::file::ofmCreateAlways,
					CommonLib::file::aeReadWrite, CommonLib::file::smNoMode, CommonLib::file::oftBinary);

				uint32_t header[3] = { Magic, Version, storage::BlockSize };
				WriteAll(ptrFile, header, sizeof(header));

				uint32_t nCount = 0;
				std::vector<uint32_t> crcs;
				pages.ForEachPage([&](int64_t nAddr, const byte_t* pData, uint32_t nSize)
				{
					uint32_t nCrc = util::CCRC::Crc32(pData, nSize);
					WriteAll(ptrFile, &nAddr, sizeof(nAddr));
					WriteAll(ptrFile, &nSize, sizeof(nSize));
					WriteAll(ptrFile, &nCrc, sizeof(nCrc));
					WriteAll(ptrFile, pData, nSize);
					crcs.push_back(nCrc);
					++nCount;
				});

				int64_t nMark = CommitMark;
				uint32_t nTotalCrc = crcs.empty() ? 0 : util::CCRC::Crc32((const byte_t*)crcs.data(), (uint32_t)(crcs.size() * sizeof(uint32_t)));
				WriteAll(ptrFile, &nMark, sizeof(nMark));
				WriteAll(ptrFile, &nCount, sizeof(nCount));
				WriteAll(ptrFile, &nTotalCrc, sizeof(nTotalCrc));
				ptrFile->Flush();        // the commit point
				ptrFile->CloseFile();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("[CRedoLog] failed to write the redo log", excSrc);
				throw;
			}
		}

		bool CRedoLog::Recover(storage::IStorageIOPtr ptrStorage, const std::wstring& sPath)
		{
			try
			{
				if (!CommonLib::CFileUtils::IsFileExist(sPath))
					return false;

				bool bReplayed = false;
				{
					CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFile(sPath.c_str(), CommonLib::file::ofmOpenExisting,
						CommonLib::file::arRead, CommonLib::file::smNoMode, CommonLib::file::oftBinary);

					uint32_t header[3] = { 0, 0, 0 };
					bool bComplete = false;
					struct SLogPage
					{
						int64_t  nAddr;
						uint32_t nSize;
						uint64_t nPos;      // of the data
					};
					std::vector<SLogPage> pages;
					if (ReadAll(ptrFile, header, sizeof(header)) && header[0] == Magic)
					{
						if (header[1] != Version)
							throw CommonLib::CExcBase("the redo log of version {0}, supported {1}", header[1], Version);
						if (header[2] != storage::BlockSize)
							throw CommonLib::CExcBase("the log has blocks of {0} bytes, the database {1}", header[2], storage::BlockSize);

						// the pages up to the commit record; a torn or missing commit record: not committed
						std::vector<byte_t> data;
						std::vector<uint32_t> crcs;
						for (;;)
						{
							int64_t nAddr = 0;
							if (!ReadAll(ptrFile, &nAddr, sizeof(nAddr)))
								break;
							if (nAddr == CommitMark)
							{
								uint32_t nCount = 0, nTotalCrc = 0;
								if (ReadAll(ptrFile, &nCount, sizeof(nCount)) && ReadAll(ptrFile, &nTotalCrc, sizeof(nTotalCrc)) && nCount == crcs.size())
								{
									uint32_t nCrc = crcs.empty() ? 0 : util::CCRC::Crc32((const byte_t*)crcs.data(), (uint32_t)(crcs.size() * sizeof(uint32_t)));
									bComplete = nCrc == nTotalCrc;
								}
								break;
							}
							uint32_t nSize = 0, nCrc = 0;
							if (!ReadAll(ptrFile, &nSize, sizeof(nSize)) || nSize == 0 || (nSize % storage::BlockSize) != 0 || nSize > storage::PageRef::MaxPageSize())
								break;
							uint64_t nPos = ptrFile->GetFilePos() + sizeof(nCrc);
							data.resize(nSize);
							if (!ReadAll(ptrFile, &nCrc, sizeof(nCrc)) || !ReadAll(ptrFile, data.data(), nSize))
								break;
							if (util::CCRC::Crc32(data.data(), nSize) != nCrc)
								break;
							crcs.push_back(nCrc);
							pages.push_back(SLogPage{ nAddr, nSize, nPos });
						}

						if (bComplete)
						{
							// the pages allocated by the commit may lie past the end of the file
							int64_t nEnd = 0;
							for (const auto& page : pages)
								nEnd = (std::max)(nEnd, page.nAddr + (int64_t)(page.nSize / storage::BlockSize));
							if (storage::CFileStorage* pFile = dynamic_cast<storage::CFileStorage*>(ptrStorage.get()))
								pFile->ReserveAddr(nEnd);

							for (const auto& page : pages)
							{
								data.resize(page.nSize);
								ptrFile->SetFilePos64(page.nPos, CommonLib::soFromBegin);
								if (!ReadAll(ptrFile, data.data(), page.nSize))
									throw CommonLib::CExcBase("failed to read the page {0} of the log", page.nAddr);
								ptrStorage->WriteData(page.nAddr, data.data(), page.nSize);
							}
							bReplayed = true;
						}
					}
					ptrFile->CloseFile();
				}

				if (bReplayed)
				{
					storage::IStorage* pStorage = dynamic_cast<storage::IStorage*>(ptrStorage.get());
					if (pStorage)
						pStorage->Flush();
				}
				CommonLib::CFileUtils::DelFile(sPath);
				return bReplayed;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("[CRedoLog] failed to recover the database from the redo log", excSrc);
				throw;
			}
		}

		void CRedoLog::Delete(const std::wstring& sPath)
		{
			if (CommonLib::CFileUtils::IsFileExist(sPath))
				CommonLib::CFileUtils::DelFile(sPath);
		}
	}
}
