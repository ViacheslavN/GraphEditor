#pragma once

#include "../Database.h"

namespace CommonLib
{
	namespace database
	{

		class CCryptoContextHolder
		{
		public:
			static CCryptoContextHolder& Instance();

			ICryptoContextPtr GetCryptoContext(const astr& databaseName) const;
			void AddCryptoContext(const astr& databaseName, ICryptoContextPtr ptrContext);
			void RemoveCryptoContext(const astr& databaseName);

			~CCryptoContextHolder();

		private:
			CCryptoContextHolder();
			typedef std::map<astr, ICryptoContextPtr> TMapCryptoContexts;
			TMapCryptoContexts m_cryptoContexts;
		};



	}
}
