#pragma once

#include "BaseSocketIP.h"

namespace CommonLib
{
	namespace network
	{
		typedef std::shared_ptr<class CAcceptedSocket> AcceptedSocketPtr;

		class CAcceptedSocket : public CBaseSocketIP
		{
		public:
			CAcceptedSocket(SocketType socket, const astr& connectedAddr);
			virtual ~CAcceptedSocket();
			const astr GetClientAddr() const;

		private:
			astr m_connectedAddr;

		};
	}
}