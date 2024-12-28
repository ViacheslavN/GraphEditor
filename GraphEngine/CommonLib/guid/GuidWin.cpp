#include "stdafx.h"
#include "GuidWin.h"
#include "../exception/WinExc.h"

namespace CommonLib
{
	namespace win
	{
		CGuid::CGuid()
		{
			m_guid = guid_null;
		}

		CGuid::CGuid(const CGuid &gid)
		{
			m_guid = gid.m_guid;
		}

		CGuid::CGuid(const GUID gid)
		{
			m_guid = gid;
		}

		CGuid::CGuid(const astr &gid)
		{
			FromAstr(gid);
		}

		CGuid::CGuid(const wstr &gid)
		{
			FromWstr(gid);
		}

		void CGuid::FromAstr(const astr& gid)
		{
			const char* pStr = gid.c_str();
			if (pStr != nullptr)
			{
				if (*pStr == '{')
					++pStr;
			}		

			RPC_STATUS res = UuidFromStringA((RPC_CSTR)pStr, (UUID *)&m_guid);
			if (res != RPC_S_OK)
				throw CWinExc("Failed to convert astr: {0} to guid", gid, res);
		}

		void CGuid::FromWstr(const wstr& gid)
		{
			const wchar_t* pStr = gid.c_str();
			if (pStr != nullptr)
			{
				if (*pStr == L'{')
					++pStr;
			}

			RPC_STATUS res = UuidFromStringW((RPC_WSTR)pStr, (UUID *)&m_guid);
			if (res != RPC_S_OK)
				throw CWinExc(L"Failed to convert astr: {0} to guid", gid, res);
		}

		astr CGuid::ToAstr(bool withbrackets) const
		{
			RPC_CSTR rpcs;
			astr result;
			if (UuidToStringA(&m_guid, &rpcs) != RPC_S_OK)
				return result;
			
			result.assign(rpcs, rpcs + std::strlen((char *)rpcs));
			RpcStringFreeA(&rpcs);
	
			if (withbrackets)
				return "{" + result + "}";

			return result;
		}

		wstr CGuid::ToWstr(bool withbrackets) const
		{
			RPC_WSTR rpcs;
			wstr result;
			if (UuidToStringW(&m_guid, &rpcs) != RPC_S_OK)
				return result;
		
			result.assign(rpcs, rpcs + +std::wcslen((wchar_t *)rpcs));
			RpcStringFreeW(&rpcs);

			if (withbrackets)
				return L"{" + result + L"}";

			return result;
		}

		CGuid CGuid::CreateNew()
		{
			CGuid guid;
			::CoCreateGuid(&guid.m_guid);

			return guid;
		}

		CGuid  CGuid::CreateNull()
		{
			return CGuid(guid_null);
		}
		
		CGuid& CGuid::operator =(const CGuid& gid)
		{
			m_guid = gid.m_guid;
			return *this;
		}

		CGuid::~CGuid()
		{

		}

		bool CGuid::operator ==(const CGuid &gid) const
		{
			return memcmp((const void*)&m_guid, (const void*)&gid.m_guid, sizeof(m_guid)) == 0;
		}

		bool CGuid::operator !=(const CGuid &gid) const
		{
			return memcmp((const void*)&m_guid, (const void*)&gid.m_guid, sizeof(m_guid)) != 0;
		}

		bool CGuid::operator <(const CGuid &gid) const
		{
			return memcmp((const void*)&m_guid, (const void*)&gid.m_guid, sizeof(m_guid)) < 0;
		}

		bool CGuid::operator >(const CGuid &gid) const
		{
			return memcmp((const void*)&m_guid, (const void*)&gid.m_guid, sizeof(m_guid)) > 0;
		}

		bool CGuid::operator <=(const CGuid &gid) const
		{
			return memcmp((const void*)&m_guid, (const void*)&gid.m_guid, sizeof(m_guid)) <= 0;
		}

		bool CGuid::operator >=(const CGuid &gid) const
		{
			return memcmp((const void*)&m_guid, (const void*)&gid.m_guid, sizeof(m_guid)) >= 0;
		}
	}
}