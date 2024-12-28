#include "stdafx.h"
#include "AcceptedSocket.h"


namespace CommonLib
{
	namespace network
	{

		CAcceptedSocket::CAcceptedSocket(SocketType socket, const astr& connectedAddr) : CBaseSocketIP( socket),
			m_connectedAddr(connectedAddr)
		{

		}

		CAcceptedSocket::~CAcceptedSocket()
		{

		}

		const astr CAcceptedSocket::GetClientAddr() const
		{
			return m_connectedAddr;
		}
	}
}