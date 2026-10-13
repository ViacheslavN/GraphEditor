#include "Database.h"
#include "Transactions/Transaction.h"
#include "Transactions/RedoLog.h"
#include "../UtilsLib/CRC.h"
#include "../../CommonLib/alloc/simpleAlloc.h"
#include "../../CommonLib/filesystem/filesystem.h"
#include "../../CommonLib/str/StringEncoding.h"

namespace bptreedb
{
	IDatabasePtr CreateDatabase()
	{
		return std::make_shared<db::CDatabase>();
	}

	namespace db
	{
		namespace
		{
			const char HeaderMagic[8] = { 'B', 'P', 'T', 'R', 'E', 'E', 'D', 'B' };

			std::wstring RedoPath(const std::wstring& sPath)
			{
				return sPath + L"-redo";
			}

			std::wstring TranPath(const std::wstring& sPath)
			{
				return sPath + L"-tran";
			}

			void CheckParams(const SDatabaseParams& params)
			{
				if (params.m_nPageSize < 8192 || params.m_nPageSize > 1024 * 1024 || (params.m_nPageSize & (params.m_nPageSize - 1)) != 0)
					throw CommonLib::CExcBase("page size {0} isn't a power of 2 in 8K..1M", params.m_nPageSize);
				if (params.m_nTreeCacheSize < 4)
					throw CommonLib::CExcBase("tree cache size {0} is less than 4", params.m_nTreeCacheSize);
				if (params.m_nSpatialCacheSize < 4)
					throw CommonLib::CExcBase("spatial index cache size {0} is less than 4", params.m_nSpatialCacheSize);
				if (params.m_nPageCacheSize < 4)
					throw CommonLib::CExcBase("page cache size {0} is less than 4", params.m_nPageCacheSize);
				if (params.m_nTranMemoryPages < 1)
					throw CommonLib::CExcBase("transaction memory pages is 0");
				if (params.m_nCompressLevel > 9)
					throw CommonLib::CExcBase("compress level {0} is more than 9", params.m_nCompressLevel);
			}
		}

		CDatabase::CDatabase()
		{
		}

		CDatabase::~CDatabase()
		{
			try
			{
				if (IsOpen())
					CloseStorage();
			}
			catch (...)
			{
			}
		}

		void CDatabase::Create(const std::string& sPathUtf8, const SDatabaseParams& params)
		{
			Create(CommonLib::StringEncoding::str_utf82w_safe(sPathUtf8), params);
		}

		void CDatabase::Open(const std::string& sPathUtf8, const SDatabaseParams& params)
		{
			Open(CommonLib::StringEncoding::str_utf82w_safe(sPathUtf8), params);
		}

		void CDatabase::Create(const std::wstring& sPath, const SDatabaseParams& params)
		{
			std::lock_guard<std::mutex> locker(m_stateMutex);
			try
			{
				if (m_ptrContext.get())
					throw CommonLib::CExcBase("the database is open");
				CheckParams(params);

				CRedoLog::Delete(RedoPath(sPath));
				CRedoLog::Delete(TranPath(sPath));

				// the header first (a new file), the storage keeps its pages after it: the schema is the first page
				const int64_t nSchemaPage = 0;
				WriteHeader(sPath, params.m_nPageSize, nSchemaPage);
				OpenStorage(sPath, params, params.m_nPageSize);

				if (m_ptrContext->m_ptrPager->GetNewFilePageAddr() != nSchemaPage)
					throw CommonLib::CExcBase("the storage of a new file isn't empty");
				m_ptrSchema = std::make_shared<CSchema>(m_ptrContext, nSchemaPage);
				m_ptrSchema->Save(m_ptrContext->m_ptrPager, true);
				m_ptrContext->m_ptrPager->Flush();
				m_ptrContext->m_bOpen = true;
			}
			catch (std::exception& excSrc)
			{
				try { CloseStorage(); } catch (...) {}
				CommonLib::CExcBase::RegenExcT("failed to create database {0}", CommonLib::StringEncoding::str_w2utf8_safe(sPath), excSrc);
				throw;
			}
		}

		void CDatabase::Open(const std::wstring& sPath, const SDatabaseParams& params)
		{
			std::lock_guard<std::mutex> locker(m_stateMutex);
			try
			{
				if (m_ptrContext.get())
					throw CommonLib::CExcBase("the database is open");
				CheckParams(params);

				uint32_t nPageSize = 0;
				int64_t nSchemaPage = -1;
				ReadHeader(sPath, nPageSize, nSchemaPage);
				OpenStorage(sPath, params, nPageSize);

				// a redo log left by a crash: complete - the commit is replayed, incomplete - deleted
				CRedoLog::Recover(m_ptrFileStorage, RedoPath(sPath));
				CRedoLog::Delete(TranPath(sPath));

				m_ptrSchema = std::make_shared<CSchema>(m_ptrContext, nSchemaPage);
				m_ptrSchema->Load();
				m_ptrContext->m_bOpen = true;
			}
			catch (std::exception& excSrc)
			{
				try { CloseStorage(); } catch (...) {}
				CommonLib::CExcBase::RegenExcT("failed to open database {0}", CommonLib::StringEncoding::str_w2utf8_safe(sPath), excSrc);
				throw;
			}
		}

		void CDatabase::OpenStorage(const std::wstring& sPath, const SDatabaseParams& params, uint32_t nPageSize)
		{
			m_sPath = sPath;
			m_bBroken = false;
			SDBContextPtr ptrContext = std::make_shared<SDBContext>();
			ptrContext->m_ptrAlloc = std::make_shared<CommonLib::CSimpleAlloc>();
			ptrContext->m_sPath = sPath;
			ptrContext->m_nPageSize = nPageSize;
			ptrContext->m_nTranMemoryPages = params.m_nTranMemoryPages;
			ptrContext->m_nTreeCacheSize = params.m_nTreeCacheSize;
			ptrContext->m_nSpatialCacheSize = params.m_nSpatialCacheSize;
			ptrContext->m_nCompressLevel = params.m_nCompressLevel;

			m_ptrFileStorage = std::make_shared<storage::CFileStorage>(ptrContext->m_ptrAlloc, 1, storage::IStorageCipherPtr());
			m_ptrFileStorage->Open(sPath.c_str(), false, HeaderSize, nPageSize);
			ptrContext->m_ptrPager = std::make_shared<storage::CFilePager>(ptrContext->m_ptrAlloc, m_ptrFileStorage, params.m_nPageCacheSize);
			m_ptrContext = ptrContext;
		}

		void CDatabase::WriteHeader(const std::wstring& sPath, uint32_t nPageSize, int64_t nSchemaPage)
		{
			try
			{
				byte_t header[HeaderSize] = {};
				size_t nPos = 0;
				memcpy(header, HeaderMagic, sizeof(HeaderMagic)); nPos += sizeof(HeaderMagic);
				uint32_t nVersion = FormatVersion;
				memcpy(header + nPos, &nVersion, sizeof(nVersion)); nPos += sizeof(nVersion);
				memcpy(header + nPos, &nPageSize, sizeof(nPageSize)); nPos += sizeof(nPageSize);
				memcpy(header + nPos, &nSchemaPage, sizeof(nSchemaPage)); nPos += sizeof(nSchemaPage);
				uint32_t nCrc = util::CCRC::Crc32(header, (uint32_t)nPos);
				memcpy(header + nPos, &nCrc, sizeof(nCrc));

				// a new file (the storage opens it afterwards)
				CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFile(sPath.c_str(), CommonLib::file::ofmCreateAlways,
					CommonLib::file::aeReadWrite, CommonLib::file::smNoMode, CommonLib::file::oftBinary);
				if (ptrFile->Write(header, HeaderSize) != (std::streamsize)HeaderSize)
					throw CommonLib::CExcBase("failed to write {0} bytes", HeaderSize);
				ptrFile->Flush();
				ptrFile->CloseFile();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("failed to write the header of the database", excSrc);
				throw;
			}
		}

		void CDatabase::ReadHeader(const std::wstring& sPath, uint32_t& nPageSize, int64_t& nSchemaPage)
		{
			try
			{
				if (!CommonLib::CFileUtils::IsFileExist(sPath))
					throw CommonLib::CExcBase("the file doesn't exist");

				CommonLib::file::TFilePtr ptrFile = CommonLib::file::CFileCreator::OpenFile(sPath.c_str(), CommonLib::file::ofmOpenExisting,
					CommonLib::file::arRead, (CommonLib::file::enShareMode)(CommonLib::file::smRead | CommonLib::file::smWrite), CommonLib::file::oftBinary);
				byte_t header[HeaderSize] = {};
				if (ptrFile->Read(header, HeaderSize) != (std::streamsize)HeaderSize)
					throw CommonLib::CExcBase("the file is too small");
				ptrFile->CloseFile();

				if (memcmp(header, HeaderMagic, sizeof(HeaderMagic)) != 0)
					throw CommonLib::CExcBase("not a bptreedb database");

				size_t nPos = sizeof(HeaderMagic);
				uint32_t nVersion = 0;
				memcpy(&nVersion, header + nPos, sizeof(nVersion)); nPos += sizeof(nVersion);
				memcpy(&nPageSize, header + nPos, sizeof(nPageSize)); nPos += sizeof(nPageSize);
				memcpy(&nSchemaPage, header + nPos, sizeof(nSchemaPage)); nPos += sizeof(nSchemaPage);
				uint32_t nCrc = 0;
				memcpy(&nCrc, header + nPos, sizeof(nCrc));

				if (nCrc != util::CCRC::Crc32(header, (uint32_t)nPos))
					throw CommonLib::CExcBase("wrong crc of the header");
				if (nVersion != FormatVersion)
					throw CommonLib::CExcBase("unsupported format version {0}", nVersion);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("failed to read the header of the database", excSrc);
				throw;
			}
		}

		void CDatabase::Close()
		{
			std::lock_guard<std::mutex> locker(m_stateMutex);
			try
			{
				if (!m_ptrContext.get())
					return;
				if (m_nActiveTransactions != 0)
					throw CommonLib::CExcBase("{0} transaction(s) are active", m_nActiveTransactions);
				CloseStorage();
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("failed to close the database", excSrc);
				throw;
			}
		}

		void CDatabase::CloseStorage()
		{
			if (m_ptrContext.get())
				m_ptrContext->m_bOpen = false;
			if (m_ptrSchema.get())
				m_ptrSchema->SetClosed();
			m_ptrSchema.reset();

			if (m_ptrContext.get() && m_ptrContext->m_ptrPager.get())
				m_ptrContext->m_ptrPager->Flush();
			if (m_ptrFileStorage.get())
				m_ptrFileStorage->Close();

			m_ptrFileStorage.reset();
			m_ptrContext.reset();
		}

		bool CDatabase::IsOpen() const
		{
			return m_ptrContext.get() != nullptr && m_ptrContext->m_bOpen;
		}

		ISchemaPtr CDatabase::GetSchema() const
		{
			return Schema();
		}

		CSchemaPtr CDatabase::Schema() const
		{
			if (!IsOpen())
				throw CommonLib::CExcBase("the database isn't open");
			return m_ptrSchema;
		}

		ITransactionPtr CDatabase::StartTransaction(eTransactionDataType type, eTransactionType tranType)
		{
			try
			{
				if (!IsOpen())
					throw CommonLib::CExcBase("the database isn't open");
				if (tranType == eWAL)
					throw CommonLib::CExcBase("eWAL transactions aren't implemented yet");
				if (tranType != eTT2PL && tranType != eTTDirectRead)
					throw CommonLib::CExcBase("wrong transaction type {0}", (int)tranType);
				if (type != eTT_SELECT && type != eTT_MODIFY && type != eTT_DDL)
					throw CommonLib::CExcBase("wrong transaction data type {0}", (int)type);
				if (tranType == eTTDirectRead && type != eTT_SELECT)
					throw CommonLib::CExcBase("a direct read transaction is read only (eTT_SELECT)");

				CTransactionPtr ptrTran = std::make_shared<CTransaction>(shared_from_this(), type, tranType);
				ptrTran->Begin();
				return ptrTran;
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("failed to start a transaction", excSrc);
				throw;
			}
		}

		void CDatabase::TransactionStarted()
		{
			std::lock_guard<std::mutex> locker(m_stateMutex);
			if (!IsOpen())
				throw CommonLib::CExcBase("the database isn't open");
			++m_nActiveTransactions;
		}

		void CDatabase::TransactionFinished()
		{
			std::lock_guard<std::mutex> locker(m_stateMutex);
			--m_nActiveTransactions;
		}

		void CDatabase::CheckUsable() const
		{
			if (m_bBroken)
				throw CommonLib::CExcBase("a commit failed to write the database file: close and open the database (its redo log is replayed)");
		}

		void CDatabase::ApplyCommit(CTranStorageIO& pages, bool bExclusive)
		{
			try
			{
				CheckUsable();
				if (pages.PrivatePageCount() == 0)
					return;

				// the commit point: the log with the commit record is on the disk (readers go on meanwhile)
				const std::wstring sRedo = RedoPath(m_sPath);
				CRedoLog::Write(sRedo, pages);
				if (m_afterRedoLog)
					m_afterRedoLog();

				std::unique_lock<std::shared_mutex> exclusive;
				if (!bExclusive)
					exclusive = std::unique_lock<std::shared_mutex>(m_stateLock);     // waits for the readers

				try
				{
					storage::IStorageIOPtr ptrIO = m_ptrContext->m_ptrPager->GetStorageIO();
					pages.ForEachPage([&ptrIO](int64_t nAddr, const byte_t* pData)
					{
						ptrIO->WriteData(nAddr, pData, ptrIO->GetPageSize());
					});
					m_ptrContext->m_ptrPager->Flush();
					CRedoLog::Delete(sRedo);
				}
				catch (...)
				{
					// committed (the log is complete), but the file and the page cache may have a part of the
					// pages: the next Open replays the log
					m_bBroken = true;
					throw;
				}
				++m_ptrContext->m_nCommits;      // the shared trees of the tables are opened again
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExc("failed to apply the commit", excSrc);
				throw;
			}
		}
	}
}
