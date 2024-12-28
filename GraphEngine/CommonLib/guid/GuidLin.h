#pragma once
# include "libuuid/uuid.h"
namespace CommonLib
{
	namespace lin
	{
		class CGuid
		{
		public:
			CGuid();
			CGuid(const CGuid &gid);
			CGuid(const uuid_t gid);
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
			uuid_t	m_guid;
		};
	}

	typedef lin::CGuid CGuid;
}