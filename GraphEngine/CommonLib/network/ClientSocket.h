#pragma once

#include "BaseSocketIP.h"

namespace CommonLib
{
	namespace network
	{
		typedef std::shared_ptr<class CClientSocket> CClientSocketPtr;

		class CClientSocket : public CBaseSocketIP
		{
		public:
			CClientSocket();
			virtual ~CClientSocket();


			void Connect(const CSocketAddr& addr);
			void ConnectToIP(const astr& addr, unsigned short port);
			void ConnectToHost(const astr& hostName, unsigned short port);

			const astr& GetEndPoint() const;

		private:
			astr m_connectdAddr;
		};
	}
}