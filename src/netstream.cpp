/*
 *      Copyright (C) 2013 Viktor PetroFF
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <iostream>
#include "Ws2tcpip.h"
#include "platform/util/StdString.h"
#include "utils.h"
#include "comstream.h"
#include "netstream.h"

using namespace std;
using namespace ADDON;

#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

#define UDP_TX_BUF_SIZE 32768
#define UDP_MAX_PKT_SIZE 65536

namespace LibNetStream 
{

class CUdpStream : public ComImplStream
{
friend class INetStreamFactory;

    static ULONG sm_ulMCastIf;

	CUdpStream(const CStdString& strUrl)
	{
		_iOffsetPkt = 0;
		_iLengthPkt = 0;

		ULONG ulMCastIf = (INADDR_NONE == sm_ulMCastIf)?INADDR_ANY:sm_ulMCastIf;
		CStdString strLocal;
		string strHostName;
		int iPort = 1234;

		int ndx = strUrl.Find("://");
		if (ndx > 0)
		{

			CStdString strProt = strUrl.Mid(0, ndx);
			CStdString strHost = strUrl.Mid(ndx + 3);
			strHost.TrimLeft('@');

			vector<string> arrHost;
			Tokenize(strHost, arrHost, "@");

			if(2 == arrHost.size())
			{
				strLocal = arrHost[0];
				strHost = arrHost[1];
			}

			arrHost.clear();
			Tokenize(strHost, arrHost, ":");

			ndx = 0;
			if (ndx < arrHost.size())
			{
				strHostName = arrHost[ndx++];
			}

			if (ndx < arrHost.size())
			{
				iPort = atoi(arrHost[ndx++].c_str());
			}
		}
		else
		{
			throw invalid_argument("strUrl");
		}

		if(!strLocal.IsEmpty())
		{
			vector<string> arrHost;
			Tokenize(strLocal, arrHost, ":");

			ulMCastIf = inet_addr(arrHost[0].c_str());
		}

		_ulMCastIf = ulMCastIf;

		_sd = INVALID_SOCKET;
		_sd = socket(AF_INET, SOCK_DGRAM, 0);
		//0 indicates that the default protocol for the type selected is to be used.
		//For example, IPPROTO_TCP is chosen for the protocol if the type  was set to
		//SOCK_STREAM and the address family is AF_INET.

		if (_sd == INVALID_SOCKET)
		{
			ThrowException("socket");
		}

		memset (&_sockaddr, 0, sizeof( _sockaddr ) );
		_sockaddr.sin_family = AF_INET;
		_sockaddr.sin_port = htons(iPort);
		_sockaddr.sin_addr.s_addr = inet_addr(strHostName.c_str());

		int is_multicast = is_multicast_address((struct sockaddr*) &_sockaddr);

		if(!is_multicast)
		{
			closesocket(_sd);
			throw invalid_argument("Ip address is not multicast.");
		}

		bool reuse_socket=true;
        if (setsockopt(_sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse_socket, sizeof(reuse_socket)) != 0)
		{
			closesocket(_sd);
			ThrowException("setsockopt SO_REUSEADDR");
		}

		int len = sizeof(_sockaddr);
        int bind_ret = ::bind(_sd, (struct sockaddr *)&_sockaddr, len);

		struct sockaddr_in addr=_sockaddr;
		//addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_addr.s_addr = _ulMCastIf;

		if (bind_ret < 0 && ::bind(_sd, (struct sockaddr *)&addr, len) < 0)
		{
			closesocket(_sd);
			ThrowException("bind");
		}

        if (udp_join_multicast_group(_sd, (struct sockaddr *)&_sockaddr) < 0)
		{
			closesocket(_sd);
			ThrowException("udp_join_multicast_group");
		}

		int tmp = UDP_MAX_PKT_SIZE;
        if (setsockopt(_sd, SOL_SOCKET, SO_RCVBUF, (const char *)&tmp, sizeof(tmp)) < 0)
		{
			closesocket(_sd);
			ThrowException("setsockopt SO_RCVBUF");
        }

		fd_set fds;
		struct timeval tv={0};

		// Set up the file descriptor set.
		FD_ZERO(&fds) ;
		FD_SET(_sd, &fds) ;

		// Set up the struct timeval for the timeout.
		tv.tv_sec = 14;

		// Wait until timeout or data received.
		int rc = select (_sd, &fds, NULL, NULL, &tv ) ;
		if (!rc)
		{
			closesocket(_sd);
			SetLastError(WSAETIMEDOUT);
			ThrowException("Initial read operation timed out.");
		}
		else if( SOCKET_ERROR == rc )
		{
			ThrowException("Initial read operation error.");
		}
		//XBMC->Log(LOG_DEBUG, "OK!!!!!");
	}

	virtual ~CUdpStream()
	{
		if (INVALID_SOCKET != _sd)
		{
			udp_leave_multicast_group(_sd, (struct sockaddr *)&_sockaddr);
			closesocket(_sd);
			_sd = INVALID_SOCKET;
			_ulMCastIf = INADDR_ANY;
			_iOffsetPkt = 0;
			_iLengthPkt = 0;
		}
	}

protected:

	int is_multicast_address(struct sockaddr *addr)
	{
		if (addr->sa_family == AF_INET)
		{
			return IN_MULTICAST(ntohl(((struct sockaddr_in *)addr)->sin_addr.s_addr));
		}
	#if HAVE_STRUCT_SOCKADDR_IN6
		if (addr->sa_family == AF_INET6)
		{
			return IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6 *)addr)->sin6_addr);
		}
	#endif

		return 0;
	}


	int udp_set_multicast_ttl(int sockfd, int mcastTTL,
									 struct sockaddr *addr)
	{
		int rc = 0;

	#ifdef IP_MULTICAST_TTL
		if (addr->sa_family == AF_INET)
		{
			rc = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, (const char *)&mcastTTL, sizeof(mcastTTL));
		}
	#endif
	#if defined(IPPROTO_IPV6) && defined(IPV6_MULTICAST_HOPS)
		if (addr->sa_family == AF_INET6)
		{
			rc = setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &mcastTTL, sizeof(mcastTTL));
		}
	#endif

		return rc;
	}

	int udp_join_multicast_group(int sockfd, struct sockaddr *addr)
	{
		int rc = 0;

	#ifdef IP_ADD_MEMBERSHIP
		if (addr->sa_family == AF_INET)
		{
			struct ip_mreq mreq;

			mreq.imr_multiaddr.s_addr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
			mreq.imr_interface.s_addr= _ulMCastIf;
			rc = setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&mreq, sizeof(mreq));
		}
	#endif
	#if HAVE_STRUCT_IPV6_MREQ && defined(IPPROTO_IPV6)
		if (addr->sa_family == AF_INET6)
		{
			struct ipv6_mreq mreq6;

			memcpy(&mreq6.ipv6mr_multiaddr, &(((struct sockaddr_in6 *)addr)->sin6_addr), sizeof(struct in6_addr));
			mreq6.ipv6mr_interface= 0;
			rc = setsockopt(sockfd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0);
		}
	#endif

		return rc;
	}

	int udp_leave_multicast_group(int sockfd, struct sockaddr *addr)
	{
		int rc = 0;

	#ifdef IP_DROP_MEMBERSHIP
		if (addr->sa_family == AF_INET)
		{
			struct ip_mreq mreq;

			mreq.imr_multiaddr.s_addr = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
			mreq.imr_interface.s_addr= _ulMCastIf;
			rc = setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&mreq, sizeof(mreq));
		}
	#endif
	#if HAVE_STRUCT_IPV6_MREQ && defined(IPPROTO_IPV6)
		if (addr->sa_family == AF_INET6)
		{
			struct ipv6_mreq mreq6;

			memcpy(&mreq6.ipv6mr_multiaddr, &(((struct sockaddr_in6 *)addr)->sin6_addr), sizeof(struct in6_addr));
			mreq6.ipv6mr_interface= 0;
			rc = setsockopt(sockfd, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0);
		}
	#endif
		return rc;
	}

public:

    virtual HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead)
    {
		HRESULT hr = S_OK;

		if (INVALID_SOCKET != _sd)
		{
			PBYTE buffer = reinterpret_cast<PBYTE>(pv);
			int count = cb;
			int offset = 0;

			while (count > 0)
			{
				int iBuffLen = ReadBuffer(buffer, offset, count);
				if(iBuffLen > 0 )
				{
					offset += iBuffLen;
					count -= iBuffLen;
				}
				else
				{
					break;
				}
			}

			if(pcbRead)
			{
				*pcbRead = static_cast<ULONG>(offset);
			}

			if(count > 0)
			{
				hr = S_FALSE;
			}
		}
		else
		{
			//throw new ObjectDisposedException("AccessVLCObject");
			hr = HRESULT_FROM_WIN32(ERROR_OBJECT_NOT_FOUND);
		}

		return hr;
    }

	CStdString errormessage( int errnum, const char* functionname) const
	{
	  const char* errmsg = NULL;

	  switch (errnum)
	  {
	  case WSANOTINITIALISED:
		errmsg = "A successful WSAStartup call must occur before using this function.";
		break;
	  case WSAENETDOWN:
		errmsg = "The network subsystem or the associated service provider has failed";
		break;
	  case WSA_NOT_ENOUGH_MEMORY:
		errmsg = "Insufficient memory available";
		break;
	  case WSA_INVALID_PARAMETER:
		errmsg = "One or more parameters are invalid";
		break;
	  case WSA_OPERATION_ABORTED:
		errmsg = "Overlapped operation aborted";
		break;
	  case WSAEINTR:
		errmsg = "Interrupted function call";
		break;
	  case WSAEBADF:
		errmsg = "File handle is not valid";
		break;
	  case WSAEACCES:
		errmsg = "Permission denied";
		break;
	  case WSAEFAULT:
		errmsg = "Bad address";
		break;
	  case WSAEINVAL:
		errmsg = "Invalid argument";
		break;
	  case WSAENOTSOCK:
		errmsg = "Socket operation on nonsocket";
		break;
	  case WSAEDESTADDRREQ:
		errmsg = "Destination address required";
		break;
	  case WSAEMSGSIZE:
		errmsg = "Message too long";
		break;
	  case WSAEPROTOTYPE:
		errmsg = "Protocol wrong type for socket";
		break;
	  case WSAENOPROTOOPT:
		errmsg = "Bad protocol option";
		break;
	  case WSAEPFNOSUPPORT:
		errmsg = "Protocol family not supported";
		break;
	  case WSAEAFNOSUPPORT:
		errmsg = "Address family not supported by protocol family";
		break;
	  case WSAEADDRINUSE:
		errmsg = "Address already in use";
		break;
	  case WSAECONNRESET:
		errmsg = "Connection reset by peer";
		break;
	  case WSAHOST_NOT_FOUND:
		errmsg = "Authoritative answer host not found";
		break;
	  case WSATRY_AGAIN:
		errmsg = "Nonauthoritative host not found, or server failure";
		break;
	  case WSAEISCONN:
		errmsg = "Socket is already connected";
		break;
	  case WSAETIMEDOUT:
		errmsg = "Connection timed out";
		break;
	  case WSAECONNREFUSED:
		errmsg = "Connection refused";
		break;
	  case WSANO_DATA:
		errmsg = "Valid name, no data record of requested type";
		break;
	  default:
		errmsg = "WSA Error";
	  }

	  CStdString strErr;
	  strErr.Format("%s: (Winsock error=%i) %s", functionname, errnum, errmsg);

	  return strErr;
	}

	int getLastError() const
	{
	  return WSAGetLastError();
	}
protected:

	void ThrowException(LPCSTR szFuncName)
	{
		int iErrCode = getLastError();
		throw system_error(static_cast<error_code::value_type>(iErrCode), system_category(), errormessage(iErrCode, szFuncName).c_str());
	}

	int ReadBuffer(PBYTE buffer, int offset, int count)
	{
		int iLen = ReadFromLastBlock(buffer, offset, count);

		offset += iLen;
		count -= iLen;

		if (count > 0)
		{
			int read = 2800;
			int len = recv(_sd, _packetBuff, read, 0);
			if (len > 0)
			{
				int iBufferLen = min(count, len);
				memcpy(buffer + offset, _packetBuff, iBufferLen);
				iLen += iBufferLen;
				count -= iBufferLen;

				if (count > 0 || len == iBufferLen)
				{
					_iOffsetPkt = 0;
					_iLengthPkt = 0;
				}
				else
				{
					_iOffsetPkt = iBufferLen;
					_iLengthPkt = len;
				}
			}
		}

		//XBMC->Log(LOG_DEBUG, "%s - iLen: %u", __FUNCTION__, iLen);

		return iLen;
	}

	int ReadFromLastBlock(PBYTE buffer, int offset, int count)
	{
		int iLen = 0;
		if (_iLengthPkt)
		{
			PCHAR pBuffer = _packetBuff + _iOffsetPkt;
			int iLenAvaible=(_iLengthPkt - _iOffsetPkt);
			iLen = min(count, iLenAvaible);
			memcpy(buffer + offset, pBuffer, iLen);
			_iOffsetPkt += iLen;

			if (_iOffsetPkt == _iLengthPkt)
			{
				_iOffsetPkt = 0;
				_iLengthPkt = 0;
			}
		}

		return iLen;
	}

private:
    SOCKET _sd;
    SOCKADDR_IN _sockaddr;
	ULONG _ulMCastIf;
	CHAR _packetBuff[UDP_MAX_PKT_SIZE+4];
	int _iOffsetPkt;
	int _iLengthPkt;
};

ULONG CUdpStream::sm_ulMCastIf = INADDR_NONE;

class CHttpStream : public ComImplStream
{
friend class INetStreamFactory;

	CHttpStream(const CStdString& strUrl)
	{
		_streamHandle = XBMC->OpenFile(strUrl, 0);
		if (NULL == _streamHandle)
		{
			throw system_error(static_cast<error_code::value_type>(-1), system_category(), strUrl.c_str());
		}
	}

	virtual ~CHttpStream()
	{
		if (_streamHandle)
		{
			XBMC->CloseFile(_streamHandle);
			_streamHandle = NULL;
		}
	}

public:
    virtual HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead)
    {
		HRESULT hr = S_OK;

		if (NULL != _streamHandle)
		{
			ULONG count = XBMC->ReadFile(_streamHandle, pv, cb);

			if(pcbRead)
			{
				*pcbRead = count;
			}

			if(count < cb)
			{
				hr = S_FALSE;
			}
		}
		else
		{
			//throw new ObjectDisposedException("AccessVLCObject");
			hr = HRESULT_FROM_WIN32(ERROR_OBJECT_NOT_FOUND);
		}

		return hr;
    }

private:
	void* _streamHandle;
};

void INetStreamFactory::SetMCastIf(ULONG ulMCastIf)
{
	CUdpStream::sm_ulMCastIf = ulMCastIf;
}

IStream* INetStreamFactory::NewStream(const CStdString& strUrl)
{
	IStream* pstream = NULL;

	try
	{
		if(strUrl.length() > 4 && "udp:" == strUrl.substr(0,4))
		{
			pstream = new CUdpStream(strUrl);
		}
		else
		{
			pstream = new CHttpStream(strUrl);
		}
	}
	catch(const exception& excp)
	{
		XBMC->Log(LOG_ERROR, "Open network stream failed, error: %s", excp.what());
	}

	return pstream;
}

} //namespace LibHttpStream 
