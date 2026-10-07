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

		CGuid::CGuid(const std::string &gid)
		{
			FromAstr(gid);
		}

		CGuid::CGuid(const std::wstring &gid)
		{
			FromWstr(gid);
		}

		void CGuid::FromAstr(const std::string& gid)
		{
			// accept both "xxxxxxxx-..." and "{xxxxxxxx-...}" (ToAstr(true) writes brackets)
			std::string str = gid;
			if (!str.empty() && str.front() == '{')
				str.erase(0, 1);
			if (!str.empty() && str.back() == '}')
				str.pop_back();

			RPC_STATUS res = UuidFromStringA((RPC_CSTR)str.c_str(), (UUID *)&m_guid);
			if (res != RPC_S_OK)
				throw CWinExc("Failed to convert std::string: {0} to guid", gid, res);
		}

		void CGuid::FromWstr(const std::wstring& gid)
		{
			const wchar_t* pStr = gid.c_str();
			if (pStr != nullptr)
			{
				if (*pStr == L'{')
					++pStr;
			}

			RPC_STATUS res = UuidFromStringW((RPC_WSTR)pStr, (UUID *)&m_guid);
			if (res != RPC_S_OK)
				throw CWinExc(L"Failed to convert std::string: {0} to guid", gid, res);
		}

        std::string CGuid::ToAstr(bool withbrackets) const
		{
			RPC_CSTR rpcs;
            std::string result;
			if (UuidToStringA(&m_guid, &rpcs) != RPC_S_OK)
				return result;
			
			result.assign(rpcs, rpcs + std::strlen((char *)rpcs));
			RpcStringFreeA(&rpcs);
	
			if (withbrackets)
				return "{" + result + "}";

			return result;
		}

        std::wstring CGuid::ToWstr(bool withbrackets) const
		{
			RPC_WSTR rpcs;
            std::wstring result;
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

        CGuid CGuid::CreateFromAstr(const std::string& guid)
        {
            return CGuid(guid);
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