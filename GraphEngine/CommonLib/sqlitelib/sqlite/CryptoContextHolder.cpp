#include "stdafx.h"
#include "CryptoContextHolder.h"
#include "../../exception/exc_base.h"

namespace CommonLib
{
	namespace database
	{

		void ICryptoContext::AddCryptoContext(const astr& databaseName, ICryptoContextPtr ptrCryptoContex)
		{
			CCryptoContextHolder::Instance().AddCryptoContext(databaseName, ptrCryptoContex);
		}

		void ICryptoContext::RemoveCryptoContext(const astr& databaseName)
		{
			CCryptoContextHolder::Instance().RemoveCryptoContext(databaseName);
		}


		CCryptoContextHolder& CCryptoContextHolder::Instance()
		{
			static CCryptoContextHolder cryptoContext;
			return cryptoContext;
		}

		CCryptoContextHolder::CCryptoContextHolder()
		{}

		CCryptoContextHolder::~CCryptoContextHolder()
		{}

		ICryptoContextPtr CCryptoContextHolder::GetCryptoContext(const astr& databaseName) const
		{
			auto it = m_cryptoContexts.find(databaseName);
			if (it != m_cryptoContexts.end())
				return it->second;

			return ICryptoContextPtr();
		}

		void CCryptoContextHolder::AddCryptoContext(const astr& databaseName, ICryptoContextPtr ptrContext)
		{
			auto it = m_cryptoContexts.find(databaseName);
			if (it != m_cryptoContexts.end())
				throw CExcBase("CryptoContext has been set already");

			m_cryptoContexts.insert(std::make_pair(databaseName, ptrContext));
		}

		void CCryptoContextHolder::RemoveCryptoContext(const astr& databaseName)
		{
			m_cryptoContexts.erase(databaseName);
		}


		

	}
}