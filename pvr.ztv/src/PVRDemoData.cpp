/*
 *      Copyright (C) 2014 Viktor PetroFF
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
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

#include <algorithm>
#include <functional>
#include "tinyxml/tinyxml.h"
#include "utils.h"
#include "netstream.h"
#include "LibVLCPlugin.h"
#include "PVRDemoData.h"


using namespace std;
using namespace ADDON;
using namespace LibVLCCAPlugin;
using namespace LibNetStream;

struct AlphabeticalOrderLess : public std::binary_function <PVRDemoChannel, PVRDemoChannel, bool>
{
    bool operator()(
        const PVRDemoChannel& _Left, 
        const PVRDemoChannel& _Right
    ) const
	{
		CStdStringW strLeftNameW = UTF8Util::ConvertUTF8ToUTF16(_Left.strChannelName.c_str()).Trim();
		CStdStringW strRightNameW = UTF8Util::ConvertUTF8ToUTF16(_Right.strChannelName.c_str()).Trim();

		return (strLeftNameW.CompareNoCase(strRightNameW) < 0);
	}
};

struct IpAddressOrderLess : public std::binary_function <PVRDemoChannel, PVRDemoChannel, bool>
{
    bool operator()(
        const PVRDemoChannel& _Left, 
        const PVRDemoChannel& _Right
    ) const
	{
		return (_Left.ulIpNumber < _Right.ulIpNumber);
	}
};

struct URLOrderLess : public std::binary_function <PVRDemoChannel, PVRDemoChannel, bool>
{
    bool operator()(
        const PVRDemoChannel& _Left, 
        const PVRDemoChannel& _Right
    ) const
	{
		return (_Left.strStreamURL.compare(_Right.strStreamURL) < 0);
	}
};

const int HTTP_OK = 200;
const int HTTP_NOTFOUND = 404;

const char* PVRDemoData::ZTV_CASERVER_URI  = "https://ares:FXa0skl4d@new.watch-tv.zet/";
const char* PVRDemoData::ZTV_EPGSERVER_URI = "http://ares:FXa0skl4d@epg.watch-tv.zet/";

PVRDemoData::PVRDemoData(bool bIsEnableOnLineEpg, LPCSTR lpszMCastIf)
{
  m_iEpgStart = -1;
  m_strDefaultIcon = GetIconPath("515");
  m_ptrVLCCAModule = NULL;
  m_currentStream = NULL;
  m_currentChannel.iUniqueId = 0;
  m_ulMCastIf = INADDR_NONE;
  m_bIsEnableOnLineEpg = bIsEnableOnLineEpg;
  m_bCaSupport = false;

  if(lpszMCastIf && '\0' != *lpszMCastIf && strcmp("255.255.255.255", lpszMCastIf))
  {
    m_ulMCastIf = inet_addr(lpszMCastIf);

	if (m_ulMCastIf == INADDR_NONE)
	{
		XBMC->Log(LOG_ERROR, "inet_addr failed and returned INADDR_NONE");
	}   
    
	if (m_ulMCastIf == INADDR_ANY)
	{
		XBMC->Log(LOG_ERROR, "inet_addr failed and returned INADDR_ANY");
		m_ulMCastIf = INADDR_NONE;
	}
  }

  LibNetStream::INetStreamFactory::SetMCastIf(m_ulMCastIf);
}

PVRDemoData::~PVRDemoData(void)
{
  FreeVLC();
}

bool PVRDemoData::VLCInit(LPCSTR lpszCA)
{
	if(!m_ptrVLCCAModule)
	{
		m_ptrVLCCAModule = ILibVLCModule::NewModule(g_strClientPath, lpszCA, m_ulMCastIf);
	}

	return (NULL != m_ptrVLCCAModule);
}

void PVRDemoData::FreeVLC(void)
{
	if(m_currentStream)
	{
		m_currentStream->Release();
		m_currentStream = NULL;
	}

	if(m_ptrVLCCAModule)
	{
		ILibVLCModule::DeleteModule(m_ptrVLCCAModule);
		m_ptrVLCCAModule = NULL;
	}
}

void PVRDemoData::ProxyAddrInit(LPCSTR lpszIP, int iPort, bool bCaSupport)
{
	m_bCaSupport = bCaSupport;

	if(80 == iPort)
	{
		m_strProxyAddr = lpszIP;
	}
	else
	{
		m_strProxyAddr.Format("%s:%d", lpszIP, iPort);
	}
}

bool PVRDemoData::LoadChannelsData(const std::string& strM3uPath, bool bIsOnLineSource, bool bIsEnableOnLineGroups, EChannelsSort sortby)
{
	bool bSuccess = false;

	string logoCfgFile = g_strClientPath;
	if (logoCfgFile.at(logoCfgFile.size() - 1) == '\\' ||
		logoCfgFile.at(logoCfgFile.size() - 1) == '/')
	{
		logoCfgFile.append("canal_list.py");
	}
	else
	{
		logoCfgFile.append("\\canal_list.py");
	}

	void* hFile = XBMC->OpenFile(logoCfgFile.c_str(), 0);
	if (hFile != NULL)
	{
		CStdString buffer;

		while (XBMC->ReadFileString(hFile, buffer.SetBuf(256), 256))
		{
			buffer.RelBuf();
			int ndx = buffer.Find(':');
			if(ndx > 0)
			{
				CStdString name, id;

				name = buffer.Left(ndx);
				id = buffer.Mid(ndx + 1);

				int ndxBegin = name.Find('\"');
				int ndxEnd = name.ReverseFind('\"');
				if(ndxBegin > -1 && ndxEnd > ndxBegin)
				{
					name = name.Mid(ndxBegin + 1, ndxEnd - ndxBegin - 1);
				}
				else
				{
					name.Empty();
				}

				int iId = 0;

				ndxBegin = id.Find('\"');
				ndxEnd = id.ReverseFind('\"');
				if(ndxBegin > -1 && ndxEnd > ndxBegin)
				{
					id = id.Mid(ndxBegin + 1, ndxEnd - ndxBegin - 1);
					iId = atoi(id);
				}

				typedef pair <wstring, int> StrIntPair;

				if(!name.IsEmpty() && iId > 0)
				{
					CStdStringW nameW = UTF8Util::ConvertUTF8ToUTF16(name.c_str());
					m_mapLogo.insert(StrIntPair(nameW.ToLower(), iId));
				}
			}
		}

		XBMC->Log(LOG_DEBUG, "%s - insert to map %u channel to id matches", __FUNCTION__, m_mapLogo.size());

		XBMC->CloseFile(hFile);
	}
	else
	{
		XBMC->Log(LOG_ERROR, "can not open %s for read", logoCfgFile.c_str());
	}

	if(bIsOnLineSource)
	{
		bSuccess = ("00:00:00:00:00:00" == strM3uPath)?true:LoadWebXmlData(strM3uPath, bIsEnableOnLineGroups);
	}
	else
	{
		bSuccess = LoadM3UList(strM3uPath);
	}

	if (bSuccess && !m_channels.empty())
	{
		switch (sortby) 
		{
		case EChannelsSort::none:
			break;
		case EChannelsSort::id:
			std::sort(m_channels.begin(), m_channels.end());
			break;
		case EChannelsSort::name:
			std::sort(m_channels.begin(), m_channels.end(), AlphabeticalOrderLess());
			break;
		case EChannelsSort::ip:
			std::sort(m_channels.begin(), m_channels.end(), IpAddressOrderLess());
			break;
		case EChannelsSort::uri:
			std::sort(m_channels.begin(), m_channels.end(), URLOrderLess());
			break;
		default:
			break;
		}

		int iCurrentGroupId = 0;

		for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
		{
			PVRDemoChannel &channel = m_channels.at(iChannelPtr);
			channel.iChannelNumber = iChannelPtr + 1;

			iCurrentGroupId = channel.iGroupId;
			if (iCurrentGroupId > 0) 
			{
				m_groups.at(iCurrentGroupId - 1).members.push_back(channel.iChannelNumber);
			}
		}
	}


	if(bSuccess)
	{
		XBMC->QueueNotification(QUEUE_INFO, "%d channels loaded.", m_channels.size());
	}

	return bSuccess;
}

typedef const_mem_fun1_ref_t<int, const CStdString, typename CStdString::value_type> TStrFindFunc1;
typedef binder1st<TStrFindFunc1> TStrFindFunc1Binder;

class charfinder: unary_function<typename CStdString::value_type, bool>
{
public:
	charfinder(const TStrFindFunc1Binder& binder):_FuncBinder(binder){}
    result_type operator()(argument_type ch)
    {
        return (result_type)(_FuncBinder(ch) > -1);
    }
private:
	const TStrFindFunc1Binder& _FuncBinder;
};

std::string PVRDemoData::GetIconPath(LPCSTR lpszIcoFName) const
{
	string iconFile = g_strClientPath;
	string iconName = lpszIcoFName;

	string::size_type pos = iconName.find(':');
	if(string::npos == pos || iconName.size() < (pos + 4) || ('\\' != iconName[pos + 1] && '/' != iconName[pos + 1]))
	{
		iconName.erase(iconName.begin(),
			find_if(iconName.begin(), iconName.end(), NotSpace<string::value_type>()));
		string::const_reverse_iterator it = find_if(iconName.rbegin(), iconName.rend(), NotSpace<string::value_type>());
		iconName.erase(!(it == iconName.rend()) ? iconName.find_last_of(*it) + 1 : 0);

		const CStdString chars("\\/:*?|<>");
		TStrFindFunc1 funcFind(&CStdString::Find);
		TStrFindFunc1Binder binderFind(funcFind, chars);
		charfinder findPred(binderFind);

		replace_if(iconName.begin(), iconName.end(), findPred, '~') ;


		char chSlash;
		if ((chSlash = *iconFile.crbegin()) == '\\' || chSlash == '/')
		{
			chSlash = *iconFile.crbegin();
			iconFile.erase(iconFile.length()-1);
		}
		else
		{
			chSlash = '\\';
		}

		iconFile.append(1, chSlash);
		iconFile.append("Icons");
		iconFile.append(1, chSlash);
		iconFile.append(iconName);
		iconFile.append(".png");


		if (!XBMC->FileExists(iconFile.c_str(), false))
		{
			CStdStringW strNameW = UTF8Util::ConvertUTF8ToUTF16(iconName.c_str());
			std::map<std::wstring, int>::const_iterator pos = m_mapLogo.find(strNameW.Trim().ToLower());
			if(m_mapLogo.end() != pos && pos->second > 0)
			{
				iconFile = g_strClientPath;
				if ((chSlash = *iconFile.crbegin()) == '\\' || chSlash == '/')
				{
					iconFile.erase(iconFile.length()-1);
				}
				else
				{
					chSlash = '\\';
				}

				iconFile.append(1, chSlash);
				iconFile.append("Logo");
				iconFile.append(1, chSlash);

				CStdString strName;
				strName.Format("%.3d.png", pos->second);
	  
				iconFile.append(strName.c_str());
			}
			else
			{
				iconFile = m_strDefaultIcon;
			}
		}
	}
	else if (!XBMC->FileExists(iconName.c_str(), false))
	{
		iconFile = m_strDefaultIcon;
	}

	//XBMC->Log(LOG_DEBUG, "%s - icon path: %s", __FUNCTION__, iconFile.c_str());

	return iconFile;
}

bool PVRDemoData::LoadWebXmlData(const std::string& strMac, bool bIsEnableOnLineGroups)
{
  bool bSuccess = true;

  if(bIsEnableOnLineGroups)
  {
	  bSuccess = LoadWebXmlGroups();
  }

  if(bSuccess)
  {
	  bSuccess = LoadWebXmlChannels(strMac);
  }
  
  return bSuccess;
}

bool PVRDemoData::LoadWebXmlGroups()
{
	bool bSuccess = true;
	CStdString strUrl = ZTV_CASERVER_URI;
	CStdString reqWebXml = "<?xml version='1.0' encoding='utf-8'?>"\
		"<dxp.packet version='1.0' type='get_iptv_topics' label='%label%' />";
	CStdString respWebXml;
	TiXmlDocument xmlDoc;
	TiXmlElement* pRootElement;

	if (HTTP_NOTFOUND == DoHttpRequest(strUrl, reqWebXml, respWebXml) || respWebXml.IsEmpty())
	{
		XBMC->Log(LOG_ERROR, "Web xml topics not found at resource 'new.watch-tv.zet'");
		bSuccess = false;
	}
	else
	{
		xmlDoc.Parse(respWebXml, 0, TIXML_ENCODING_UTF8);
		if (xmlDoc.Error())
		{
			XBMC->Log(LOG_ERROR, "invalid web xml topics (no/invalid data responce found at 'new.watch-tv.zet', xml: '%s')", respWebXml.c_str());
			bSuccess = false;
		}

		if(bSuccess)
		{
			int iNotResult = 0;
			LPCSTR szCode = "unknown";
			pRootElement = xmlDoc.RootElement();
			if (strcmp(pRootElement->Value(), "dxp.packet") != 0
				|| (iNotResult = strcmp(pRootElement->Attribute("type"), "result")) != 0
				|| (szCode = pRootElement->Attribute("code"))
				)
			{
				LPCSTR szErrorCode;
				if(iNotResult && (szErrorCode = pRootElement->Attribute("code")))
				{
					szCode = szErrorCode;
				}

				XBMC->Log(LOG_ERROR, "error web data responce (error code '%s' found)", szCode);
				bSuccess = false;
			}
		}
	}

	if(bSuccess)
	{
		/* load channel groups */
		int iUniqueGroupId = 0;
		TiXmlElement* pElement = pRootElement->FirstChildElement("topics");
		if (pElement)
		{
			TiXmlNode *pGroupNode = NULL;
			while ((pGroupNode = pElement->IterateChildren(pGroupNode)) != NULL)
			{
				const TiXmlElement* pGrouplElement = pGroupNode->ToElement();
				LPCSTR lpszTmp = NULL;
				CStdString strTmp;
				PVRDemoChannelGroup group;
				group.iGroupId = ++iUniqueGroupId;

				/* group name */
				if (lpszTmp = pGrouplElement->Attribute("name"))
				{
					group.strGroupName = lpszTmp;
				}
				else
				{
					continue;
				}

				int iGroupId = 0;
				if (!(lpszTmp = pGrouplElement->Attribute("id", &iGroupId)) || group.iGroupId != iGroupId)
				{
					XBMC->Log(LOG_ERROR, "invalid channel group (no/invalid group id found at '%d')", iGroupId);
					continue;
				}

				group.bRadio = false;

				//XBMC->Log(LOG_DEBUG, "%s - group name: %s, id: %d", __FUNCTION__, group.strGroupName.c_str(), group.iGroupId);
				m_groups.push_back(group);
			}
		}
		else
		{
			XBMC->Log(LOG_ERROR, "Element 'topics' not found, xml: %s", respWebXml.c_str());
			bSuccess = false;
		}
	}

	return bSuccess;
}

bool PVRDemoData::LoadWebXmlChannels(const std::string& strMac)
{
  bool bSuccess = true;
  CStdString strUrl = ZTV_CASERVER_URI;
  CStdString respWebXml;
  CStdString reqWebXml;
  TiXmlDocument xmlDoc;
  TiXmlElement* pRootElement;

  reqWebXml.Format(
	  "<?xml version='1.0' encoding='utf-8'?>"\
	  "<dxp.packet version='1.0' type='get_iptv_udev_data' label='%%label%%' hw_addr='%s' show_unavailable_channels='yes' />",
	  strMac.c_str());
	  //m_ptrVLCCAModule->GetBestMacAddress().c_str());

  if (HTTP_NOTFOUND == DoHttpRequest(strUrl, reqWebXml, respWebXml) || respWebXml.IsEmpty())
  {
    XBMC->Log(LOG_ERROR, "Xml web data list not found at resource 'new.watch-tv.zet'");
    bSuccess = false;
  }
  else
  {
	  xmlDoc.Parse(respWebXml, 0, TIXML_ENCODING_UTF8);
	  if (xmlDoc.Error())
	  {
		XBMC->Log(LOG_ERROR, "invalid web xml list (no/invalid data responce found at 'new.watch-tv.zet')");
		bSuccess = false;
	  }

	  int iNotResult = 0;
	  LPCSTR szCode = "unknown";
	  pRootElement = xmlDoc.RootElement();
	  if (strcmp(pRootElement->Value(), "dxp.packet") != 0
		  || (iNotResult = strcmp(pRootElement->Attribute("type"), "result")) != 0
		  || (szCode = pRootElement->Attribute("code"))
		  )
	  {
		LPCSTR szErrorCode;
		if(iNotResult && (szErrorCode = pRootElement->Attribute("code")))
		{
			szCode = szErrorCode;
		}

		XBMC->Log(LOG_ERROR, "error web data responce (error code '%s' found)", szCode);
		bSuccess = false;
	  }
  }

  if(bSuccess)
  {
	  /* load channels */
	  int iUniqueChannelId = 0;
	  TiXmlElement* pElement = pRootElement->FirstChildElement("channels");
	  if (pElement)
	  {
		TiXmlNode *pChannelNode = NULL;
		while ((pChannelNode = pElement->IterateChildren(pChannelNode)) != NULL)
		{
		  const TiXmlElement* pChannelElement = pChannelNode->ToElement();
		  LPCSTR lpszTmp = NULL;
		  CStdString strTmp;
		  PVRDemoChannel channel = {false, 0, 0, 0, 0, false, 0, std::string(), std::string(), std::string()};

		  /* channel name */
		  if (lpszTmp = pChannelElement->Attribute("name"))
		  {
			channel.strChannelName = lpszTmp;
		  }
		  else
		  {
			continue;
		  }

		  /* CAID */
		  bool bYes = false;
		  if (TIXML_SUCCESS == pChannelElement->QueryBoolAttribute("encrypted", &bYes))
		  {
			if(bYes)
			{
				if(m_ptrVLCCAModule || m_bCaSupport)
				{
					channel.iEncryptionSystem = 1;
				}
				else
				{
					continue;
				}
			}

		  }

		  channel.strIconPath = GetIconPath(channel.strChannelName.c_str());

		  /* channel number */
		  int iChannelNumber = 0;
		  if (lpszTmp = pChannelElement->Attribute("id", &iChannelNumber))
		  {
			if(iChannelNumber > 0)
			{
				channel.iChannelNumber = iChannelNumber;
			}
		  }

		  channel.ulIpNumber = INADDR_NONE;

		  /* stream url */
		  if (lpszTmp = pChannelElement->Attribute("source"))
		  {
			strTmp = lpszTmp;
			int ndx = strTmp.Find(':');
			if(ndx > 0)
			{
				strTmp = strTmp.Left(ndx);
			}

			unsigned long ulAddr = inet_addr(strTmp);
			if ( ulAddr == INADDR_NONE )
			{
				XBMC->Log(LOG_ERROR, "inet_addr failed and returned INADDR_NONE");
			}   
    
			if (ulAddr == INADDR_ANY)
			{
				XBMC->Log(LOG_ERROR, "inet_addr failed and returned INADDR_ANY");
				ulAddr = INADDR_NONE;
			}

			LPCSTR lpcszProto=(1 == channel.iEncryptionSystem)?"ca":"udp";
			if(m_strProxyAddr.empty())
			{
				strTmp.Format("%s://@%s", lpcszProto, lpszTmp);
			}
			else
			{
				strTmp.Format("http://%s/%s/%s", m_strProxyAddr.c_str(), lpcszProto, lpszTmp);
				channel.bIsTcpTransport = true;
			}

			channel.strStreamURL = strTmp;
			channel.ulIpNumber = ulAddr;
		  }
		  else
		  {
			channel.strStreamURL = m_strDefaultMovie;
		  }

		  /* members */
		  if(!m_groups.empty())
		  {
			int iTopicID = 0;
			if (lpszTmp = pChannelElement->Attribute("topic_id", &iTopicID))
			{
				if(iTopicID <= m_groups.size())
				{
					//XBMC->Log(LOG_DEBUG, "topic: %d - id member: %d", iTopicID, channel.iUniqueId);
					//m_groups.at(iTopicID-1).members.push_back(channel.iChannelNumber);
					channel.iGroupId = iTopicID;
				}
			}
		  }

		  channel.iUniqueId = GetChannelId(channel.strStreamURL.c_str(), channel.ulIpNumber);

		  //XBMC->Log(LOG_DEBUG, "%s - channel name: %s, id: %d, mrl: %s", __FUNCTION__, channel.strChannelName.c_str(), channel.iUniqueId, channel.strStreamURL.c_str());
		  m_channels.push_back(channel);
		}
	  }
	  else
	  {
		XBMC->Log(LOG_ERROR, "Element 'channels' not found, xml: %s", respWebXml.c_str());
		bSuccess = false;
	  }
  }

  return bSuccess;
}


const char* M3U_START_MARKER       ="#EXTM3U";
const char* M3U_INFO_MARKER        ="#EXTINF";

const char* TVG_INFO_LOGO_MARKER   ="tvg-logo=";

const char* CHANNEL_ID_MARKER      ="id=";
const char* GROUP_NAME_MARKER      ="group-title=";
const char* RADIO_MARKER           ="radio=";

class groupfinder: unary_function<const PVRDemoChannelGroup&, bool>
{
public:
	groupfinder(const string& str):_strGroupName(str){}
    result_type operator()(argument_type group)
    {
		return (result_type)(_strGroupName == group.strGroupName);
    }
private:
	const string& _strGroupName;
};

/* open without caching. regardless to file type. */
#define READ_NO_CACHE  0x08

struct IntZero
{
	int iInt;

	IntZero()
	{
		iInt = 0;
	}
};

bool PVRDemoData::LoadM3UList(const std::string& strM3uUri)
{
	if (strM3uUri.empty())
	{
		XBMC->Log(LOG_NOTICE, "Playlist file path is not configured. Channels not loaded.");
		return false;
	}

	void* hFile = XBMC->OpenFile(strM3uUri.c_str(), 0); // READ_NO_CACHE
	if (!hFile)
	{
		XBMC->Log(LOG_ERROR, "Unable to load playlist file '%s':  file is missing or empty.", strM3uUri.c_str());
		return false;
	}

	stdext::hash_map<std::string, IntZero> mapURIs;
	/* load channels */
	bool isfirst = true;

	int iUniqueGroupId = 0;
	int iCurrentGroupId = 0;

	PVRDemoChannel channel = {false, 0, 0, 0, 0, false, 0, std::string(), std::string(), std::string()};
	CStdString strContent;
	CStdString strChnlID;
    CStdString strLine;
	
    while (int bytesRead = XBMC->ReadFile(hFile, strLine.SetBuf(256), 256))
	{
	  strLine.RelBuf(bytesRead);
      strContent.append(strLine);
	}

	std::stringstream stream(strContent);
	//while(XBMC->ReadFileString(hFile, strLine.SetBuf(512), 512))
	while(stream.getline(strLine.SetBuf(512), 512)) 
	{
		strLine.RelBuf();
		strLine.TrimRight(" \t\r\n");
		strLine.TrimLeft(" \t");
		//XBMC->Log(LOG_DEBUG, "==> %s", strLine.c_str());
		if (strLine.IsEmpty())
		{
			continue;
		}

		if (isfirst) 
		{
			isfirst = false;
			if (strLine.Left(3) == "\xEF\xBB\xBF")
			{
				strLine.Delete(0, 3);
			}
			if (strLine.Left((int)strlen(M3U_START_MARKER)) == M3U_START_MARKER) 
			{
				continue;
			}
			else
			{
				break;
			}
		}

		if (strLine.Left((int)strlen(M3U_INFO_MARKER)) == M3U_INFO_MARKER) 
		{
			CStdString	strChnlName;
			CStdString	strTvgLogo;
			CStdString	strGroupName;
			CStdString	strRadio;

			// parse line
			int iColon = (int)strLine.Find(':');
			int iComma = (int)strLine.ReverseFind(',');
			if (iColon >= 0 && iComma >= 0 && iComma > iColon) 
			{
				// parse name
				iComma++;
				strChnlName = strLine.Mid(iComma).Trim();
				channel.strChannelName = XBMC->UnknownToUTF8(strChnlName);

				// parse info
				iColon++;
				CStdString strInfoLine = strLine.Mid(iColon, --iComma - iColon);

				strChnlID = ReadMarkerValue(strInfoLine, CHANNEL_ID_MARKER);
				strTvgLogo = ReadMarkerValue(strInfoLine, TVG_INFO_LOGO_MARKER);
				strGroupName = ReadMarkerValue(strInfoLine, GROUP_NAME_MARKER);
				strRadio      = ReadMarkerValue(strInfoLine, RADIO_MARKER);

				if (strTvgLogo.IsEmpty())
				{
					strTvgLogo = strChnlName;
				}
				else
				{
					strTvgLogo = XBMC->UnknownToUTF8(strTvgLogo);
				}

				channel.strIconPath = GetIconPath(strTvgLogo);
				channel.bRadio      = !strRadio.CompareNoCase("true");

				if (!strGroupName.IsEmpty())
				{
					strGroupName = XBMC->UnknownToUTF8(strGroupName);

					vector<PVRDemoChannelGroup>::const_iterator pos = find_if(m_groups.cbegin(), m_groups.cend(), groupfinder(strGroupName));
					if(m_groups.cend() == pos)
					{
					    PVRDemoChannelGroup group;
						group.strGroupName = strGroupName;
						group.iGroupId = ++iUniqueGroupId;
						group.bRadio = channel.bRadio;

						m_groups.push_back(group);
						iCurrentGroupId = iUniqueGroupId;
					}
					else
					{
						iCurrentGroupId = pos->iGroupId;
					}
				}
			}
		} 
		else if (strLine[0] != '#')
		{
			channel.ulIpNumber = INADDR_NONE;

			bool bMcastProto = false;

			if(strLine.length() < 6)
			{
				channel.iEncryptionSystem = 0;
			}
			else if("ca:" == strLine.substr(0,3))
			{
				if(m_ptrVLCCAModule || m_bCaSupport)
				{
					channel.iEncryptionSystem = 1;
				}
				else
				{
					continue;
				}

				bMcastProto = true;
			}
			else if("udp:" == strLine.substr(0,4))
			{
				bMcastProto = true;
			}
			else if("http:" == strLine.substr(0,5))
			{
				channel.bIsTcpTransport = true;
			}

			CStdString strIp = strLine;
			int ndx = strIp.ReverseFind(':');
			if(ndx > 0 && strIp.Left(ndx).ReverseFind(':') > 0)//"1234" == strIp.Mid(ndx + 1))
			{
				int ndxx = strIp.ReverseFind('/');

				if(ndxx > 0 && ndx > ndxx)
				{
					strIp = strIp.Mid(ndxx + 1, ndx - ndxx - 1);

					strIp.TrimLeft('@');

					unsigned long ulAddr = inet_addr(strIp);
					if ( ulAddr == INADDR_NONE )
					{
						XBMC->Log(LOG_ERROR, "inet_addr failed and returned INADDR_NONE");
					}   
    
					if (ulAddr == INADDR_ANY)
					{
						XBMC->Log(LOG_ERROR, "inet_addr failed and returned INADDR_ANY");
						ulAddr = INADDR_NONE;
					}

					channel.ulIpNumber = ulAddr;
				}
			}

			if(bMcastProto)
			{
				ndx = strLine.ReverseFind('@');
				if(ndx > 0)
				{
					strIp = strLine.Mid(ndx + 1);
					LPCSTR lpcszProto = (1 == channel.iEncryptionSystem)?"ca":"udp";
					CStdString strTmp;
					if(m_strProxyAddr.empty())
					{
						strTmp.Format("%s://@%s", lpcszProto, strIp.c_str());
					}
					else
					{
						strTmp.Format("http://%s/%s/%s", m_strProxyAddr.c_str(), lpcszProto, strIp.c_str());
						channel.bIsTcpTransport = true;
					}
					channel.strStreamURL = strTmp;
				}
			}
			else
			{
				channel.strStreamURL	= strLine;
			}

			if (!strChnlID.IsEmpty())
			{
				int iChannelNumber = atoi(strChnlID);

				if(iChannelNumber > 0)
				{
					channel.iChannelNumber = iChannelNumber;
				}
			}

			channel.iUniqueId = GetChannelId(channel.strStreamURL.c_str(), channel.ulIpNumber);

			IntZero iIndex = mapURIs[channel.strStreamURL];
			mapURIs[channel.strStreamURL].iInt++;

			channel.iUniqueId += iIndex.iInt;

			if (iCurrentGroupId > 0) 
			{
				channel.iGroupId = iCurrentGroupId;
			}

			m_channels.push_back(channel);

			channel.iUniqueId		= 0;
			channel.iChannelNumber	= 0;
			channel.iGroupId        = 0;
			channel.iEncryptionSystem = 0;
			channel.bRadio = false;
			channel.ulIpNumber = 0;
			channel.bIsTcpTransport = false;
			channel.strChannelName.clear();
			channel.strStreamURL.clear();
			channel.strIconPath.clear();
		}
	}

	XBMC->CloseFile(hFile);

	if (m_channels.empty())
	{
		XBMC->Log(LOG_ERROR, "Unable to load channels from file '%s':  file is corrupted.", strM3uUri.c_str());

		return false;
	}

	XBMC->Log(LOG_NOTICE, "Loaded %d channels.", m_channels.size());

	return true;
}

int PVRDemoData::GetChannelsAmount(void)
{
  return m_channels.size();
}

PVR_ERROR PVRDemoData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  for (std::vector<PVRDemoChannel>::const_iterator pos = m_channels.begin(); pos < m_channels.end(); pos++)
  {
    const PVRDemoChannel &channel = *pos;
    if (channel.bRadio == bRadio)
    {
      PVR_CHANNEL xbmcChannel;
      memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

      xbmcChannel.iUniqueId         = channel.iUniqueId;
      xbmcChannel.bIsRadio          = channel.bRadio;
      xbmcChannel.iChannelNumber    = channel.iChannelNumber;
      strncpy(xbmcChannel.strChannelName, channel.strChannelName.c_str(), sizeof(xbmcChannel.strChannelName) - 1);
	  //strncpy(xbmcChannel.strStreamURL, channel.strStreamURL.c_str(), sizeof(xbmcChannel.strStreamURL) - 1);
	  xbmcChannel.iEncryptionSystem = channel.bIsTcpTransport?0:channel.iEncryptionSystem;
	  //PVR_STRCPY(xbmcChannel.strInputFormat, "video/x-mpegts");
	  if(channel.bRadio)
	  {
        PVR_STRCPY(xbmcChannel.strInputFormat, "audio/mpeg");
      }
	  else
      {
        PVR_STRCPY(xbmcChannel.strInputFormat, "video/x-mpegts");
	  }
      strncpy(xbmcChannel.strIconPath, channel.strIconPath.c_str(), sizeof(xbmcChannel.strIconPath) - 1);
      xbmcChannel.bIsHidden         = false;

      PVR->TransferChannelEntry(handle, &xbmcChannel);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

bool PVRDemoData::GetChannel(const PVR_CHANNEL &channel, PVRDemoChannel &myChannel)
{
  int iChanNum = channel.iChannelNumber;

  XBMC->Log(LOG_DEBUG, "Find channel UniqueId: %d, iChannelNumber: %d.", channel.iUniqueId, channel.iChannelNumber);

  if(iChanNum > 0 && iChanNum <= m_channels.size())
  {
	PVRDemoChannel& thisChannel = m_channels.at(iChanNum-1);
    if (thisChannel.iUniqueId == (int) channel.iUniqueId)
    {
      myChannel = thisChannel;

      return true;
    }
  }

  return false;
}

int PVRDemoData::GetChannelGroupsAmount(void)
{
  return m_groups.size();
}

PVR_ERROR PVRDemoData::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int iGroupPtr = 0; iGroupPtr < m_groups.size(); iGroupPtr++)
  {
    PVRDemoChannelGroup &group = m_groups.at(iGroupPtr);
    if (group.bRadio == bRadio)
    {
      PVR_CHANNEL_GROUP xbmcGroup;
      memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));

      xbmcGroup.bIsRadio = bRadio;
      strncpy(xbmcGroup.strGroupName, group.strGroupName.c_str(), sizeof(xbmcGroup.strGroupName) - 1);

      PVR->TransferChannelGroup(handle, &xbmcGroup);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRDemoData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  for (unsigned int iGroupPtr = 0; iGroupPtr < m_groups.size(); iGroupPtr++)
  {
    PVRDemoChannelGroup &myGroup = m_groups.at(iGroupPtr);
    if (!strcmp(myGroup.strGroupName.c_str(),group.strGroupName))
    {
      for (unsigned int iChannelPtr = 0; iChannelPtr < myGroup.members.size(); iChannelPtr++)
      {
        int iId = myGroup.members.at(iChannelPtr) - 1;
        if (iId < 0 || iId > (int)m_channels.size() - 1)
          continue;
        PVRDemoChannel &channel = m_channels.at(iId);
        PVR_CHANNEL_GROUP_MEMBER xbmcGroupMember;
        memset(&xbmcGroupMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

        strncpy(xbmcGroupMember.strGroupName, group.strGroupName, sizeof(xbmcGroupMember.strGroupName) - 1);
        xbmcGroupMember.iChannelUniqueId = channel.iUniqueId;
        xbmcGroupMember.iChannelNumber   = channel.iChannelNumber;
		//xbmcGroupMember.iChannelNumber   = iChannelPtr + 1;

        PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
      }
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRDemoData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
	if (m_iEpgStart == -1)
	{
		m_iEpgStart = iStart;
	}

	PVR_ERROR result = PVR_ERROR_NO_ERROR;
	time_t iLastEndTime = m_iEpgStart + 1;

	PVRDemoChannel& myChannel = m_channels.at(channel.iChannelNumber -1);

	if (m_bIsEnableOnLineEpg && iLastEndTime < iEnd)
	{
		result = RequestWebEPGForChannel(handle, myChannel, iStart, iEnd);
	}

  return result;
}

PVR_ERROR PVRDemoData::RequestWebEPGForChannel(ADDON_HANDLE handle, const PVRDemoChannel& channel, time_t iStart, time_t iEnd)
{
	CStdString strIPAddr;
	unsigned long ulIPAddr = channel.ulIpNumber;
	if (~ulIPAddr && ulIPAddr)
	{
		in_addr in={0};
		in.S_un.S_addr=ulIPAddr;
		strIPAddr = inet_ntoa(in);
	}
	else
	{
		return PVR_ERROR_NO_ERROR;
	}

	CStdString strUrl = ZTV_EPGSERVER_URI;
	CStdString reqWebXml;
	CStdString respWebXml;

	reqWebXml.Format( "<?xml version='1.0' encoding='utf-8'?>"\
		"<dxp.packet version='1.0' type='get_epg' channel_name='zetc://@%s:1234' />", strIPAddr.c_str());//'zetc://@235.10.10.7:1234' />"

	if (HTTP_NOTFOUND == DoHttpRequest(strUrl, reqWebXml, respWebXml) || respWebXml.IsEmpty())
	{
		XBMC->Log(LOG_ERROR, "epg data not found at resource 'epg.watch-tv.zet' for channel '%s'", strIPAddr.c_str());
		return PVR_ERROR_SERVER_ERROR;
	}

	TiXmlDocument xmlDoc;
	xmlDoc.Parse(respWebXml, 0, TIXML_ENCODING_UTF8);

	if (xmlDoc.Error())
	{
		XBMC->Log(LOG_ERROR, "invalid epg data (no/invalid data responce found at 'epg.watch-tv.zet', xml: '%s')", respWebXml.c_str());
		return PVR_ERROR_FAILED;
	}

	int iNotResult = 0;
	LPCSTR szCode = "unknown";
	TiXmlElement *pRootElement = xmlDoc.RootElement();
	if (strcmp(pRootElement->Value(), "dxp.packet") != 0
		|| (iNotResult = strcmp(pRootElement->Attribute("type"), "result")) != 0
		|| (szCode = pRootElement->Attribute("code"))
		)
	{
		LPCSTR szErrorCode;
		if(iNotResult && (szErrorCode = pRootElement->Attribute("code")))
		{
			szCode = szErrorCode;
		}

		XBMC->Log(LOG_ERROR, "error responce web xml data (error code '%s' found)", szCode);
		return PVR_ERROR_SERVER_ERROR;
	}

	/* load EPG entries */
	int iUniqueBroadcastId = 0;
	std::vector<PVRDemoEpgEntry> epg;
	TiXmlElement* pElement = pRootElement->FirstChildElement("programlist");
	if (pElement)
	{
		TiXmlNode *pProgramNode = NULL;
		while ((pProgramNode = pElement->IterateChildren(pProgramNode)) != NULL)
		{
			const TiXmlElement* pProgramElement = pProgramNode->ToElement();
			LPCSTR lpszTmp = NULL;
			CStdString strTmp;
			int iTmp;
			PVRDemoEpgEntry entry;

			/* start */
			if (
				(lpszTmp = pProgramElement->Attribute("start"))
				&& 0 != (iTmp = DateTimeToTimeT(lpszTmp))
				&& iTmp >= iStart
				)
			{
				entry.startTime = iTmp;
			}
			else
			{
				continue;
			}

			/* end */
			if (
				(lpszTmp = pProgramElement->Attribute("stop"))
				&& 0 != (iTmp = DateTimeToTimeT(lpszTmp))
				&& iTmp <= iEnd
				)
			{
				entry.endTime = iTmp;
			}
			else
			{
				continue;
			}

			const TiXmlNode* pNode = pProgramNode->FirstChild();
			if (pNode != NULL)
			{
				strTmp = pNode->Value();
			}

			/* broadcast id */
			entry.iBroadcastId = ++iUniqueBroadcastId;

			/* channel id */
			entry.iChannelId = channel.iUniqueId;

			/* title */
			//strTmp.Format("EPG entry #%d for channel #%d.", entry.iBroadcastId, channel.iUniqueId);
			entry.strTitle = strTmp;

			/* plot */
			entry.strPlot = strTmp;

			/* plot outline */
			entry.strPlotOutline = m_strDefaultIcon.c_str();
			entry.strIconPath = m_strDefaultIcon.c_str();

			/* icon path */
			if (lpszTmp = pProgramElement->Attribute("picture"))
			{
				entry.strIconPath = lpszTmp;
				entry.strPlotOutline = lpszTmp;
				//XBMC->Log(LOG_DEBUG, "%s - icon path: %s", __FUNCTION__, lpszTmp);
			}

			//XBMC->Log(LOG_DEBUG, "loaded EPG entry '%s' channel '%d' start '%d' end '%d'",
				//entry.strTitle.c_str(), entry.iChannelId, entry.startTime, entry.endTime);

			epg.push_back(entry);
		}

		int iAddBroadcastId = ((channel.iUniqueId >> 16) + channel.iUniqueId) << 20;

		for (unsigned int iEntryPtr = 0; iEntryPtr < epg.size(); iEntryPtr++)
		{
			const PVRDemoEpgEntry& myTag = epg.at(iEntryPtr);

			EPG_TAG tag;
			memset(&tag, 0, sizeof(EPG_TAG));

			tag.iUniqueBroadcastId = myTag.iBroadcastId + iAddBroadcastId;
			tag.strTitle           = myTag.strTitle.c_str();
			tag.iChannelNumber     = myTag.iChannelId;
			tag.startTime          = myTag.startTime;
			tag.endTime            = myTag.endTime;
			tag.strPlotOutline     = myTag.strPlotOutline.c_str();
			tag.strPlot            = myTag.strPlot.c_str();
			//tag.strIconPath        = m_strDefaultIcon.c_str();
			tag.strIconPath        = myTag.strIconPath.c_str();
			//tag.iGenreType         = myTag.iGenreType;
			//tag.iGenreSubType      = myTag.iGenreSubType;

			PVR->TransferEpgEntry(handle, &tag);
		}
	}

	return PVR_ERROR_NO_ERROR;
}

bool PVRDemoData::OpenLiveStream(const PVR_CHANNEL &channelinfo)
{
	bool bSuccess = false;

	if (GetChannel(channelinfo, m_currentChannel))
    {
		XBMC->Log(LOG_DEBUG, "OpenLiveStream(%d:%s) (oid=%d)",
			m_currentChannel.iChannelNumber, m_currentChannel.strChannelName.c_str(), m_currentChannel.iUniqueId);

		if(m_currentChannel.bIsTcpTransport || !m_ptrVLCCAModule)
		{
			m_currentStream = LibNetStream::INetStreamFactory::NewStream(m_currentChannel.strStreamURL);
		}
		else
		{
			m_currentStream = m_ptrVLCCAModule->NewAccess(m_currentChannel.strStreamURL);
		}

		bSuccess = (NULL != m_currentStream);
    }

	return bSuccess;
}

void PVRDemoData::CloseLiveStream()
{
	if(m_currentChannel.iUniqueId)
	{
		m_currentChannel.iUniqueId = 0;
		m_currentChannel.strStreamURL.clear();

		if(m_currentStream)
		{
			m_currentStream->Release();
			m_currentStream = NULL;
		}
	}
}

bool PVRDemoData::SwitchChannel(const PVR_CHANNEL &channelinfo)
{
	bool bSuccess = true;
	// if we're already on the correct channel, then dont do anything
	if (((int)channelinfo.iUniqueId) != m_currentChannel.iUniqueId)
	{
		// open new stream
		CloseLiveStream();
		bSuccess = OpenLiveStream(channelinfo);
	}

	return bSuccess;
}

int PVRDemoData::ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
	int iRead = -1;

	if(m_currentStream)
	{
		//XBMC->Log(LOG_DEBUG, "%s - iBufferSize: %u", __FUNCTION__, iBufferSize);
		iRead = 0;
		ULONG ulRead;
		HRESULT hr = m_currentStream->Read(pBuffer, static_cast<ULONG>(iBufferSize), &ulRead);
		if(SUCCEEDED(hr))
		{
			iRead = static_cast<int>(ulRead);
		}
	}

	return iRead;
}

int PVRDemoData::GetCurrentClientChannel()
{
	return m_currentChannel.iUniqueId;
}

const char * PVRDemoData::GetLiveStreamURL(const PVR_CHANNEL &channel)
{
	return m_currentChannel.strStreamURL.c_str();
}

bool PVRDemoData::CanPauseStream()
{
	return m_currentChannel.bIsTcpTransport;
}

void PVRDemoData::PauseStream(bool bPaused)
{
	XBMC->Log(LOG_DEBUG, "%s - bPaused = %u", __FUNCTION__, bPaused);
}

bool PVRDemoData::CanSeekStream()
{
	return CanPauseStream();
}


/************************************************************/
/** http handling */
int PVRDemoData::DoHttpRequest(const CStdString& resource, const CStdString& body, CStdString& response)
{
	PLATFORM::CLockObject lock(m_mutex);

	// ask XBMC to read the URL for us
	int resultCode = HTTP_NOTFOUND;

	CStdString url = resource;
	CStdString request = body;
	time_t now = time(NULL);

	struct tm *newtime = NULL;
	newtime = localtime( &now );

	if(!request.IsEmpty())
	{
		CStdString label;
		label.Format("%.4d%.2d%.2d%.2d%.2d%.2d", newtime->tm_year+1900, newtime->tm_mon+1, newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec);

		request.Replace("%label%", label);
	}

	XBMC->Log(LOG_DEBUG, "%s - request: %s", __FUNCTION__, request.c_str());
	void* hFile = XBMC->OpenFileForWrite(url.c_str(), 0);
	if (hFile != NULL)
	{
		int rc = XBMC->WriteFile(hFile, request.c_str(), request.length());
		if (rc >= 0)
		{
			CStdString result;
			CStdString buffer;

			//int iLen = 0;
			//while (iLen = XBMC->ReadFile(hFile, buffer.SetBuf(256), 256))
			while (XBMC->ReadFileString(hFile, buffer.SetBuf(256), 256))
			{
				buffer.RelBuf();
				result += buffer;
			}

			response = result;
			if(response.length() < 200)
			{
				XBMC->Log(LOG_DEBUG, "%s - response: %s", __FUNCTION__, response.c_str());
			}
			else
			{
				//XBMC->Log(LOG_DEBUG, "%s - response: %s", __FUNCTION__, response.Left(800).c_str());
			}

			resultCode = HTTP_OK;
		}
		else
		{
			XBMC->Log(LOG_ERROR, "can not write to %s", url.c_str());
		}

		XBMC->CloseFile(hFile);
	}
	else
	{
		XBMC->Log(LOG_ERROR, "can not open %s for write", url.c_str());
	}

	//XBMC->Log(LOG_DEBUG, "%s - exit --", __FUNCTION__);

	return resultCode;
}

CStdString PVRDemoData::ReadMarkerValue(std::string &strLine, const char* strMarkerName)
{
	int iMarkerStart = (int) strLine.find(strMarkerName);
	if (iMarkerStart >= 0)
	{
		std::string strMarker = strMarkerName;
		iMarkerStart += strMarker.length();
		if (iMarkerStart < (int)strLine.length())
		{
			char cFind = ' ';
			if (strLine[iMarkerStart] == '"')
			{
				cFind = '"';
				iMarkerStart++;
			}
			int iMarkerEnd = (int)strLine.find(cFind, iMarkerStart);
			if (iMarkerEnd < 0)
			{
				iMarkerEnd = strLine.length();
			}
			return strLine.substr(iMarkerStart, iMarkerEnd - iMarkerStart);
		}
	}

	return CStdString();
}

int PVRDemoData::GetChannelId(const char * strStreamUrl, unsigned int uiChannelId)
{
	const char* strString = strStreamUrl;
	SHORT wId = 0;
	SHORT c;

	while (c = *strString++)
	{
		wId = (((wId << 5) + wId) + c); /* iId * 33 + c */
	}

	if (0 == uiChannelId)
	{
		uiChannelId = 0x2;
	}
	else if(INADDR_NONE == uiChannelId)
	{
		uiChannelId = 0x3;
	}

	USHORT wChannelId = ((USHORT)(uiChannelId>>16) + (USHORT)uiChannelId);

	return ((abs(wId) << 16)|(0x18000)|(int)wChannelId);
}
