#pragma once
#include "../../exception/exc_base.h"

namespace CommonLib
{
	namespace crypto
	{
		namespace openssllib
		{
			class COpenSSLExc : public CExcBase
			{
			public:
				COpenSSLExc(const astr& err_msg);
				COpenSSLExc(int errorCode);
				COpenSSLExc(const astr& err_msg, int errorCode);

				template<typename... Types>
				COpenSSLExc(int errorCode, const astr& format, Types... args) : CExcBase(format, args...), m_nErrorCode(errorCode)
				{
					CExcBase::AddMsg(GetErrorDesc(m_nErrorCode));
				}

				virtual ~COpenSSLExc() noexcept;
				virtual std::shared_ptr<CExcBase> Clone() const;

				static astr GetErrorDesc(int errorCode);
			private:

			private:
				int m_nErrorCode;
			};
		}
	}
}