#pragma once

#include "../CommonLib.h"

namespace CommonLib
{
	namespace win
	{
		const GUID guid_null = { 0x00000000L, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
		const GUID guid_one = { 0x00000000L, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

		class CGuid
		{
		public:
			CGuid();
			CGuid(const CGuid &gid);
			CGuid(const GUID gid);
			CGuid(const astr &gid);
			CGuid(const wstr &gid);

			CGuid& operator =(const CGuid& gid);
			~CGuid();

			bool operator ==(const CGuid &gid) const;
			bool operator !=(const CGuid &gid) const;
			bool operator <(const CGuid &gid) const;
			bool operator >(const CGuid &gid) const;
			bool operator <=(const CGuid &gid) const;
			bool operator >=(const CGuid &gid) const;

			astr ToAstr(bool withbrackets) const;
			wstr ToWstr(bool withbrackets) const;

			void FromAstr(const astr& gid);
			void FromWstr(const wstr& gid);

			static CGuid CreateNew();
			static CGuid CreateNull();
		private:
			GUID m_guid;
		};
	}

	typedef win::CGuid CGuid;
}