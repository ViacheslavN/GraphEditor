#pragma once

#include "exc_base.h"
#include <comdef.h>
namespace CommonLib
{
	class CComExc : public CExcBase
	{
	public:
		CComExc(HRESULT err);
		CComExc(_com_error& err);
		CComExc(const astr& err_msg, HRESULT err);
		CComExc(const astr& err_msg, _com_error& err);

		template<typename... Types>
		CComExc(HRESULT err, const astr& format, Types&&... args) : CExcBase(format, std::forward<Types>(args)...), m_hr(err)
		{
			CExcBase::AddMsg(GetComErrorDesc(err));
		}

		~CComExc() noexcept;
		virtual std::shared_ptr<CExcBase> Clone() const;

		static astr GetComErrorDesc(HRESULT err);
	private:

	private:
		HRESULT m_hr;

	};
}