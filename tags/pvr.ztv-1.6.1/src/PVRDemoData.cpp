/*
 *      Copyright (C) 2013 Viktor PetroFF
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

#include "tinyxml/XMLUtils.h"
#include "utils.h"
#include "netstream.h"
#include "LibVLCPlugin.h"
#include "PVRDemoData.h"


using namespace std;
using namespace ADDON;
using namespace LibVLCCAPlugin;
using namespace LibNetStream;

#define HTTP_OK 200
#define HTTP_NOTFOUND 404

PVRDemoData::PVRDemoData(bool bIsEnableOnLineEpg, LPCSTR lpszMCastIf)
{
  m_iEpgStart = -1;
  m_strDefaultIcon = GetIconPath("515");
  m_ptrVLCCAModule = NULL;
  m_currentStream = NULL;
  m_currentChannel.iUniqueId = 0;
  m_bIsEnableOnLineEpg = bIsEnableOnLineEpg;

  m_ulMCastIf = INADDR_NONE;

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
  m_channels.clear();
  m_groups.clear();
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
	if(!m_ptrVLCCAModule)
	{
		ILibVLCModule::DeleteModule(m_ptrVLCCAModule);
		m_ptrVLCCAModule = NULL;
	}
}

bool PVRDemoData::LoadChannelsData(const std::string& strM3uPath, bool bIsOnLineSource, bool bIsEnableOnLineGroups)
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
		if(m_ptrVLCCAModule)
		{
			bSuccess = LoadWebXmlData(bIsEnableOnLineGroups);
		}
	}
	else
	{
		if(strM3uPath.empty())
		{
			bSuccess = LoadDemoData();
		}
		else
		{
			bSuccess = LoadM3UList(strM3uPath);
		}
	}

	if(bSuccess)
	{
		XBMC->QueueNotification(QUEUE_INFO, "%d channels loaded.", m_channels.size());
	}

	return bSuccess;
}

std::string PVRDemoData::GetSettingsFile() const
{
	string settingFile = g_strUserPath;
	if (settingFile.at(settingFile.size() - 1) == '\\' ||
		settingFile.at(settingFile.size() - 1) == '/')
	{
		settingFile.append("PVRDemoAddonSettings.xml");
	}
	else
	{
		settingFile.append("\\PVRDemoAddonSettings.xml");
	}

	if (!XBMC->FileExists(settingFile.c_str(), false))
	{
		settingFile = g_strClientPath;
		if (settingFile.at(settingFile.size() - 1) == '\\' ||
			settingFile.at(settingFile.size() - 1) == '/')
		{
			settingFile.append("PVRDemoAddonSettings.xml");
		}
		else
		{
			settingFile.append("\\PVRDemoAddonSettings.xml");
		}
	}

	return settingFile;
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

bool PVRDemoData::LoadDemoData(void)
{
  TiXmlDocument xmlDoc;
  string strSettingsFile = GetSettingsFile();

  if (!xmlDoc.LoadFile(strSettingsFile))
  {
    XBMC->Log(LOG_ERROR, "invalid demo data (no/invalid data file found at '%s')", strSettingsFile.c_str());
    return false;
  }

  TiXmlElement *pRootElement = xmlDoc.RootElement();
  if (strcmp(pRootElement->Value(), "demo") != 0)
  {
    XBMC->Log(LOG_ERROR, "invalid demo data (no <demo> tag found)");
    return false;
  }

  /* load channels */
  int iUniqueChannelId = 0;
  TiXmlElement *pElement = pRootElement->FirstChildElement("channels");
  if (pElement)
  {
    TiXmlNode *pChannelNode = NULL;
    while ((pChannelNode = pElement->IterateChildren(pChannelNode)) != NULL)
    {
      CStdString strTmp;
	  PVRDemoChannel channel;
      channel.iUniqueId = ++iUniqueChannelId;

      /* channel name */
      if (!XMLUtils::GetString(pChannelNode, "name", strTmp))
        continue;
      channel.strChannelName = strTmp;

      /* radio/TV */
      XMLUtils::GetBoolean(pChannelNode, "radio", channel.bRadio);

      /* channel number */
      if (!XMLUtils::GetInt(pChannelNode, "number", channel.iChannelNumber))
        channel.iChannelNumber = iUniqueChannelId;

      /* CAID */
      //if (!XMLUtils::GetInt(pChannelNode, "encryption", channel.iEncryptionSystem))
	    //channel.iEncryptionSystem = 0;

      /* icon path */
      if (XMLUtils::GetString(pChannelNode, "icon", strTmp))
        channel.strIconPath = strTmp;

	  string nameIcon = channel.strIconPath.empty()?channel.strChannelName:channel.strIconPath.substr(0, channel.strIconPath.length() - 4);
      channel.strIconPath = GetIconPath(nameIcon.c_str());

      /* stream url */
      if (!XMLUtils::GetString(pChannelNode, "stream", strTmp))
        channel.strStreamURL = m_strDefaultMovie;
      else
        channel.strStreamURL = strTmp;

      channel.bIsTcpTransport = false;  
	  channel.iEncryptionSystem = 0;

	  if(channel.strStreamURL.length() < 6)
	  {
		  channel.iEncryptionSystem = 0;
	  }
	  else if("ca:" == channel.strStreamURL.substr(0,3))
	  {
		  channel.iEncryptionSystem = 1;
	  }
	  else if("http:" == channel.strStreamURL.substr(0,5))
	  {
		  channel.bIsTcpTransport = true;
	  }

		channel.ulIpNumber = INADDR_NONE;

		CStdString strIp = channel.strStreamURL;
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

      m_channels.push_back(channel);
    }
  }

  /* load channel groups */
  int iUniqueGroupId = 0;
  pElement = pRootElement->FirstChildElement("channelgroups");
  if (pElement)
  {
    TiXmlNode *pGroupNode = NULL;
    while ((pGroupNode = pElement->IterateChildren(pGroupNode)) != NULL)
    {
      CStdString strTmp;
      PVRDemoChannelGroup group;
      group.iGroupId = ++iUniqueGroupId;

      /* group name */
      if (!XMLUtils::GetString(pGroupNode, "name", strTmp))
        continue;
      group.strGroupName = strTmp;

      /* radio/TV */
      XMLUtils::GetBoolean(pGroupNode, "radio", group.bRadio);

      /* members */
      TiXmlNode* pMembers = pGroupNode->FirstChild("members");
      TiXmlNode *pMemberNode = NULL;
      while (pMembers != NULL && (pMemberNode = pMembers->IterateChildren(pMemberNode)) != NULL)
      {
        int iChannelId = atoi(pMemberNode->FirstChild()->Value());
        if (iChannelId > -1)
          group.members.push_back(iChannelId);
      }

      m_groups.push_back(group);
    }
  }

#if 0
  /* load EPG entries */
  pElement = pRootElement->FirstChildElement("epg");
  if (pElement)
  {
    TiXmlNode *pEpgNode = NULL;
    while ((pEpgNode = pElement->IterateChildren(pEpgNode)) != NULL)
    {
      CStdString strTmp;
      int iTmp;
      PVRDemoEpgEntry entry;

      /* broadcast id */
      if (!XMLUtils::GetInt(pEpgNode, "broadcastid", entry.iBroadcastId))
        continue;

      /* channel id */
      if (!XMLUtils::GetInt(pEpgNode, "channelid", iTmp))
        continue;
      PVRDemoChannel &channel = m_channels.at(iTmp - 1);
      entry.iChannelId = channel.iUniqueId;

      /* title */
      if (!XMLUtils::GetString(pEpgNode, "title", strTmp))
        continue;
      entry.strTitle = strTmp;

      /* start */
      if (!XMLUtils::GetInt(pEpgNode, "start", iTmp))
        continue;
      entry.startTime = iTmp;

      /* end */
      if (!XMLUtils::GetInt(pEpgNode, "end", iTmp))
        continue;
      entry.endTime = iTmp;

      /* plot */
      if (XMLUtils::GetString(pEpgNode, "plot", strTmp))
        entry.strPlot = strTmp;

      /* plot outline */
      if (XMLUtils::GetString(pEpgNode, "plotoutline", strTmp))
        entry.strPlotOutline = strTmp;

      /* icon path */
      if (XMLUtils::GetString(pEpgNode, "icon", strTmp))
        entry.strIconPath = strTmp;

      /* genre type */
      XMLUtils::GetInt(pEpgNode, "genretype", entry.iGenreType);

      /* genre subtype */
      XMLUtils::GetInt(pEpgNode, "genresubtype", entry.iGenreSubType);

      XBMC->Log(LOG_DEBUG, "loaded EPG entry '%s' channel '%d' start '%d' end '%d'", entry.strTitle.c_str(), entry.iChannelId, entry.startTime, entry.endTime);
      channel.epg.push_back(entry);
    }
  }

  /* load recordings */
  iUniqueGroupId = 0; // reset unique ids
  pElement = pRootElement->FirstChildElement("recordings");
  if (pElement)
  {
    TiXmlNode *pRecordingNode = NULL;
    while ((pRecordingNode = pElement->IterateChildren(pRecordingNode)) != NULL)
    {
      CStdString strTmp;
      PVRDemoRecording recording;

      /* recording title */
      if (!XMLUtils::GetString(pRecordingNode, "title", strTmp))
        continue;
      recording.strTitle = strTmp;

      /* recording url */
      if (!XMLUtils::GetString(pRecordingNode, "url", strTmp))
        recording.strStreamURL = m_strDefaultMovie;
      else
        recording.strStreamURL = strTmp;

      iUniqueGroupId++;
      strTmp.Format("%d", iUniqueGroupId);
      recording.strRecordingId = strTmp;

      /* channel name */
      if (XMLUtils::GetString(pRecordingNode, "channelname", strTmp))
        recording.strChannelName = strTmp;

      /* plot */
      if (XMLUtils::GetString(pRecordingNode, "plot", strTmp))
        recording.strPlot = strTmp;

      /* plot outline */
      if (XMLUtils::GetString(pRecordingNode, "plotoutline", strTmp))
        recording.strPlotOutline = strTmp;

      /* genre type */
      XMLUtils::GetInt(pRecordingNode, "genretype", recording.iGenreType);

      /* genre subtype */
      XMLUtils::GetInt(pRecordingNode, "genresubtype", recording.iGenreSubType);

      /* duration */
      XMLUtils::GetInt(pRecordingNode, "duration", recording.iDuration);

      /* recording time */
      if (XMLUtils::GetString(pRecordingNode, "time", strTmp))
      {
        time_t timeNow = time(NULL);
        struct tm* now = localtime(&timeNow);

        int delim = strTmp.Find(':');
        if (delim != CStdString::npos)
        {
          now->tm_hour = (int)strtol(strTmp.Left(delim), NULL, 0);
          now->tm_min  = (int)strtol(strTmp.Mid(delim + 1), NULL, 0);
          now->tm_mday--; // yesterday

          recording.recordingTime = mktime(now);
        }
      }

      m_recordings.push_back(recording);
    }
  }
#endif // 0

  return true;
}

bool PVRDemoData::LoadWebXmlData(bool bIsEnableOnLineGroups)
{
  CStdString strUrl = "https://ares:FXa0skl4d@new.watch-tv.zet/";
  CStdString respWebXml;
  CStdString reqWebXml;

  if(bIsEnableOnLineGroups)
  {
	  reqWebXml = "<?xml version='1.0' encoding='utf-8'?>"\
		  "<dxp.packet version='1.0' type='get_iptv_topics' label='%label%' />";

	  if (HTTP_NOTFOUND == DoHttpRequest(strUrl, reqWebXml, respWebXml) || respWebXml.IsEmpty())
	  {
		XBMC->Log(LOG_ERROR, "Web xml topics not found at resource 'new.watch-tv.zet'");
		return false;
	  }

	  TiXmlDocument xmlDoc;
	  xmlDoc.Parse(respWebXml, 0, TIXML_ENCODING_UTF8);
	  if (xmlDoc.Error())
	  {
		XBMC->Log(LOG_ERROR, "invalid web xml topics (no/invalid data responce found at 'new.watch-tv.zet', xml: '%s')", respWebXml.c_str());
		return false;
	  }

	  LPCSTR szCode = "unknown";
	  TiXmlElement* pRootElement = xmlDoc.RootElement();
	  if (strcmp(pRootElement->Value(), "dxp.packet") != 0
		  || strcmp(pRootElement->Attribute("type"), "result") != 0
		  || (szCode = pRootElement->Attribute("code"))
		  )
	  {
		XBMC->Log(LOG_ERROR, "error web data responce (error code '%s' found)", szCode);
		return false;
	  }

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
		  //if (!XMLUtils::GetString(pGroupNode, "name", strTmp))
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

		  /* radio/TV */
		  //XMLUtils::GetBoolean(pGroupNode, "radio", group.bRadio);
		  group.bRadio = false;

		  /* members */
		  //TiXmlNode* pMembers = pGroupNode->FirstChild("members");
		  //TiXmlNode *pMemberNode = NULL;
		  //while (pMembers != NULL && (pMemberNode = pMembers->IterateChildren(pMemberNode)) != NULL)
		  //{
			//int iChannelId = atoi(pMemberNode->FirstChild()->Value());
			//if (iChannelId > -1)
			  //group.members.push_back(iChannelId);
		  //}

		  //XBMC->Log(LOG_DEBUG, "%s - group name: %s, id: %d", __FUNCTION__, group.strGroupName.c_str(), group.iGroupId);
		  m_groups.push_back(group);
		}
	  }

	  //xmlDoc.Clear();
  }

  reqWebXml.Format(
	  "<?xml version='1.0' encoding='utf-8'?>"\
	  "<dxp.packet version='1.0' type='get_iptv_udev_data' label='%%label%%' hw_addr='%s' show_unavailable_channels='yes' />",
	  m_ptrVLCCAModule->GetBestMacAddress().c_str());

  if (HTTP_NOTFOUND == DoHttpRequest(strUrl, reqWebXml, respWebXml) || respWebXml.IsEmpty())
  {
    XBMC->Log(LOG_ERROR, "Xml web data list not found at resource 'new.watch-tv.zet'");
    return false;
  }

  TiXmlDocument xmlDoc;
  xmlDoc.Parse(respWebXml, 0, TIXML_ENCODING_UTF8);
  if (xmlDoc.Error())
  {
    XBMC->Log(LOG_ERROR, "invalid web xml list (no/invalid data responce found at 'new.watch-tv.zet')");
    return false;
  }

  LPCSTR szCode = "unknown";
  TiXmlElement* pRootElement = xmlDoc.RootElement();
  if (strcmp(pRootElement->Value(), "dxp.packet") != 0
	  || strcmp(pRootElement->Attribute("type"), "result") != 0
	  || (szCode = pRootElement->Attribute("code"))
	  )
  {
    XBMC->Log(LOG_ERROR, "error web data responce (error code '%s' found)", szCode);
    return false;
  }

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
      PVRDemoChannel channel;
      channel.iUniqueId = ++iUniqueChannelId;

      /* channel name */
      //if (!XMLUtils::GetString(pChannelNode, "name", strTmp))
	  if (lpszTmp = pChannelElement->Attribute("name"))
	  {
		channel.strChannelName = lpszTmp;
	  }
	  else
	  {
        continue;
	  }

      /* radio/TV */
      //XMLUtils::GetBoolean(pChannelNode, "radio", channel.bRadio);
	  channel.bRadio = false;

      /* channel number */
      //if (!XMLUtils::GetInt(pChannelNode, "number", channel.iChannelNumber))
	  if (lpszTmp = pChannelElement->Attribute("id", &channel.iChannelNumber))
	  {
		//channel.iUniqueId = channel.iChannelNumber;
		//iUniqueChannelId = channel.iChannelNumber;
		if(channel.iUniqueId != channel.iChannelNumber)
		{
			XBMC->Log(LOG_INFO, "unique channel id (%d) and channel id/number (%d) is not match", channel.iUniqueId, channel.iChannelNumber);
		}
	  }
	  else
	  {
		channel.iChannelNumber = channel.iUniqueId;
	  }

	  channel.bIsTcpTransport = false;
      /* CAID */
      //if (!XMLUtils::GetInt(pChannelNode, "encryption", channel.iEncryptionSystem))
	  bool bYes=false;
	  if (TIXML_SUCCESS != pChannelElement->QueryBoolAttribute("encrypted", &bYes))
	  {
		channel.iEncryptionSystem = 0;
	  }
	  else
	  {
		channel.iEncryptionSystem = (bYes)?1:0;
	  }

      /* icon path */
      //if (!XMLUtils::GetString(pChannelNode, "icon", strTmp))
        //channel.strIconPath = m_strDefaultIcon;
      //else
        //channel.strIconPath = strTmp;
		
	  channel.strIconPath = GetIconPath(channel.strChannelName.c_str());

      /* stream url */
      //if (!XMLUtils::GetString(pChannelNode, "stream", strTmp))
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
		strTmp.Format("%s://@%s", lpcszProto, lpszTmp);
        channel.strStreamURL = strTmp;
		channel.ulIpNumber = ulAddr;
	  }
      else
	  {
        channel.strStreamURL = m_strDefaultMovie;
		channel.ulIpNumber = INADDR_NONE;
	  }

	  //if("ca:" == channel.strStreamURL.substr(0,3))
	  //{
		  //channel.iEncryptionSystem = -1;
	  //}

      /* members */
	  if(!m_groups.empty())
	  {
		int iTopicID = 0;
		if (lpszTmp = pChannelElement->Attribute("topic_id", &iTopicID))
		{
			if(iTopicID <= m_groups.size())
			{
				//XBMC->Log(LOG_DEBUG, "topic: %d - id member: %d", iTopicID, channel.iUniqueId);
				m_groups.at(iTopicID-1).members.push_back(channel.iUniqueId);
			}
		}
	  }

	  //XBMC->Log(LOG_DEBUG, "%s - channel name: %s, id: %d, mrl: %s", __FUNCTION__, channel.strChannelName.c_str(), channel.iUniqueId, channel.strStreamURL.c_str());
	  m_channels.push_back(channel);
    }
  }
  
  return true;
}

#define M3U_START_MARKER       "#EXTM3U"
#define M3U_INFO_MARKER        "#EXTINF"

#define TVG_INFO_LOGO_MARKER   "tvg-logo="

#define CHANNEL_ID_MARKER      "id="
#define GROUP_NAME_MARKER      "group-title="

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

bool PVRDemoData::LoadM3UList(const std::string& strM3uUri)
{
	void* hFile = XBMC->OpenFile(strM3uUri.c_str(), 0);
	if (!hFile)
	{
		XBMC->Log(LOG_ERROR, "Unable to load playlist file '%s':  file is missing or empty.", strM3uUri.c_str());
		return false;
	}

	/* load channels */
	bool isfirst = true;

	int iUniqueChannelId = 0;
	int iUniqueGroupId = 0;
	int iCurrentGroupId = 0;

	PVRDemoChannel channel;

	//char szLine[1024];
	//while(stream.getline(szLine, 1024))

    CStdString strLine;
	while(XBMC->ReadFileString(hFile, strLine.SetBuf(512), 512))
	{
		strLine.RelBuf();
		strLine.TrimRight(" \t\r\n");
		strLine.TrimLeft(" \t");

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

		CStdString	strChnlName;
		CStdString	strChnlID;
		CStdString	strTvgLogo;
		CStdString	strGroupName;

		if (strLine.Left((int)strlen(M3U_INFO_MARKER)) == M3U_INFO_MARKER) 
		{
			// parse line
			int iColon = (int)strLine.Find(':');
			int iComma = (int)strLine.ReverseFind(',');
			if (iColon >= 0 && iComma >= 0 && iComma > iColon) 
			{
				// parse name
				iComma++;
				strChnlName = strLine.Mid(iComma).Trim();
				//tmpChannel.strChannelName = XBMC->UnknownToUTF8(strChnlName);
				channel.strChannelName = XBMC->UnknownToUTF8(strChnlName);

				// parse info
				CStdString strInfoLine = strLine.Mid(++iColon, --iComma - iColon);

				strChnlID = ReadMarkerValue(strInfoLine, CHANNEL_ID_MARKER);
				strTvgLogo = ReadMarkerValue(strInfoLine, TVG_INFO_LOGO_MARKER);
				strGroupName = ReadMarkerValue(strInfoLine, GROUP_NAME_MARKER);

				if (strTvgLogo.IsEmpty())
				{
					strTvgLogo = strChnlName;
				}
				else
				{
					//tmpChannel.strTvgLogo = XBMC->UnknownToUTF8(strTvgLogo);
					strTvgLogo = XBMC->UnknownToUTF8(strTvgLogo);
				}

				channel.strIconPath = GetIconPath(strTvgLogo);


				if (!strGroupName.IsEmpty())
				{
					strGroupName = XBMC->UnknownToUTF8(strGroupName);

					//if ((pGroup = FindGroup(strGroupName)) == NULL)
					vector<PVRDemoChannelGroup>::const_iterator pos = find_if(m_groups.cbegin(), m_groups.cend(), groupfinder(strGroupName));
					if(m_groups.cend() == pos)
					{
					    PVRDemoChannelGroup group;
						group.strGroupName = strGroupName;
						group.iGroupId = ++iUniqueGroupId;
						group.bRadio = false;

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
			//PVRIptvChannel channel;
			channel.iUniqueId		= ++iUniqueChannelId;
			channel.strStreamURL	= strLine;
			channel.bIsTcpTransport = false;
			channel.iEncryptionSystem = 0;
			channel.bRadio = false;

			if(channel.strStreamURL.length() < 6)
			{
				channel.iEncryptionSystem = 0;
			}
			else if("ca:" == channel.strStreamURL.substr(0,3))
			{
				channel.iEncryptionSystem = 1;
			}
			else if("http:" == channel.strStreamURL.substr(0,5))
			{
				channel.bIsTcpTransport = true;
			}

			channel.ulIpNumber = INADDR_NONE;

			CStdString strIp = channel.strStreamURL;
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

			if (!strChnlID.IsEmpty())
			{
				//channel.iUniqueId = channel.iChannelNumber;
				//iUniqueChannelId = channel.iChannelNumber;
				channel.iChannelNumber = atoi(strChnlID);
				if(channel.iUniqueId != channel.iChannelNumber)
				{
					XBMC->Log(LOG_INFO, "unique channel id (%d) and channel id/number (%d) is not match", channel.iUniqueId, channel.iChannelNumber);
				}

				if(0 == channel.iChannelNumber)
				{
					channel.iChannelNumber = channel.iUniqueId;
				}
			}
			else
			{
				channel.iChannelNumber = channel.iUniqueId;
			}

			m_channels.push_back(channel);

			if (iCurrentGroupId > 0) 
			{
				m_groups.at(iCurrentGroupId - 1).members.push_back(channel.iChannelNumber);
			}

			//tmpChannel.strChannelName = "";
			//tmpChannel.strTvgName = "";
			//tmpChannel.strTvgLogo = "";
			channel.iUniqueId		= 0;
			channel.iChannelNumber	= 0;
			channel.iEncryptionSystem = 0;
			channel.ulIpNumber = 0;
			channel.strChannelName.clear();
			channel.strStreamURL.clear();
			channel.strIconPath.clear();
		}
	}

	if (m_channels.size() == 0)
	{
		XBMC->Log(LOG_ERROR, "Unable to load channels from file '%s':  file is corrupted.", strM3uUri.c_str());
		return false;
	}

	XBMC->CloseFile(hFile);

	XBMC->Log(LOG_NOTICE, "Loaded %d channels.", m_channels.size());
	return true;
}


int PVRDemoData::GetChannelsAmount(void)
{
  return m_channels.size();
}

PVR_ERROR PVRDemoData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  {
    PVRDemoChannel &channel = m_channels.at(iChannelPtr);
    if (channel.bRadio == bRadio)
    {
      PVR_CHANNEL xbmcChannel;
      memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

      xbmcChannel.iUniqueId         = channel.iUniqueId;
      xbmcChannel.bIsRadio          = channel.bRadio;
      xbmcChannel.iChannelNumber    = channel.iChannelNumber;
      strncpy(xbmcChannel.strChannelName, channel.strChannelName.c_str(), sizeof(xbmcChannel.strChannelName) - 1);
	  //strncpy(xbmcChannel.strStreamURL, channel.strStreamURL.c_str(), sizeof(xbmcChannel.strStreamURL) - 1);
	  xbmcChannel.iEncryptionSystem = channel.iEncryptionSystem;
	  PVR_STRCPY(xbmcChannel.strInputFormat, "video/x-mpegts");
      strncpy(xbmcChannel.strIconPath, channel.strIconPath.c_str(), sizeof(xbmcChannel.strIconPath) - 1);
      xbmcChannel.bIsHidden         = false;

      PVR->TransferChannelEntry(handle, &xbmcChannel);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

bool PVRDemoData::GetChannel(const PVR_CHANNEL &channel, PVRDemoChannel &myChannel)
{
  //for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  if(channel.iUniqueId > 0 && channel.iUniqueId <= m_channels.size())
  {
    //PVRDemoChannel &thisChannel = m_channels.at(iChannelPtr);
	PVRDemoChannel& thisChannel = m_channels.at(channel.iUniqueId-1);
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
        //xbmcGroupMember.iChannelNumber   = channel.iChannelNumber;
		xbmcGroupMember.iChannelNumber   = iChannelPtr + 1;

        PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
      }
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRDemoData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  if (m_iEpgStart == -1)
    m_iEpgStart = iStart;

  PVR_ERROR result = PVR_ERROR_NO_ERROR;
  //time_t start;
  //time_t end;
  //CDateTime::GetCurrentDateTime().GetAsUTCDateTime().GetAsTime(start);
  //end = start + m_iDisplayTime;
  //start -= g_advancedSettings.m_iEpgLingerTime * 60;

  //time_t now = time(NULL);
  //time_t offset = m_iEpgStart - (now % 604800);
  //time_t beginTime = m_iEpgStart - (m_iEpgStart % 604800) - 259200;
  time_t deltaWeek = m_iEpgStart % 604800;
  time_t beginTime = m_iEpgStart - deltaWeek + (deltaWeek < 345600)?(-259200):(345600);
  time_t iLastEndTime = m_iEpgStart + 1;
  int iAddBroadcastId = 0;

  //for (unsigned int iChannelPtr = 0; iChannelPtr < m_channels.size(); iChannelPtr++)
  //{
    //PVRDemoChannel &myChannel = m_channels.at(iChannelPtr);
    PVRDemoChannel& myChannel = m_channels.at(channel.iUniqueId -1);
    //if (myChannel.iUniqueId != (int) channel.iUniqueId)
      //continue;

    //while (iLastEndTime < iEnd && myChannel.epg.size() > 0)
	if (iLastEndTime < iEnd)
    {
#if 0
	  if(myChannel.epg.size() > 0)
	  {
		  time_t iLastEndTimeTmp = 0;
		  for (unsigned int iEntryPtr = 0; iEntryPtr < myChannel.epg.size(); iEntryPtr++)
		  {
			PVRDemoEpgEntry &myTag = myChannel.epg.at(iEntryPtr);

			EPG_TAG tag;
			memset(&tag, 0, sizeof(EPG_TAG));

			tag.iUniqueBroadcastId = myTag.iBroadcastId + iAddBroadcastId;
			tag.strTitle           = myTag.strTitle.c_str();
			tag.iChannelNumber     = myTag.iChannelId;
			tag.startTime          = myTag.startTime + beginTime;//myTag.startTime + iLastEndTime;
			tag.endTime            = myTag.endTime + beginTime;//myTag.endTime + iLastEndTime;
			tag.strPlotOutline     = myTag.strPlotOutline.c_str();
			tag.strPlot            = myTag.strPlot.c_str();
			tag.strIconPath        = myTag.strIconPath.c_str();
			tag.iGenreType         = myTag.iGenreType;
			tag.iGenreSubType      = myTag.iGenreSubType;

			iLastEndTimeTmp = tag.endTime;

			if(tag.startTime >= iStart && tag.endTime <= iEnd)
			{
				PVR->TransferEpgEntry(handle, &tag);
			}
		  }

		  iLastEndTime = iLastEndTimeTmp;
		  iAddBroadcastId += myChannel.epg.size();
	  }
	  else
#endif // 0

	  if(m_bIsEnableOnLineEpg)
	  {
		  result = RequestWebEPGForChannel(handle, myChannel, iStart, iEnd);
	  }
    }
  //}

  return result;
}


//time_t DateTimeToTimeT(const std::string& datetime);

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

	CStdString strUrl = "http://ares:FXa0skl4d@epg.watch-tv.zet/";
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

	LPCSTR szCode = "unknown";
	TiXmlElement *pRootElement = xmlDoc.RootElement();
	if (strcmp(pRootElement->Value(), "dxp.packet") != 0
		|| strcmp(pRootElement->Attribute("type"), "result") != 0
		|| (szCode = pRootElement->Attribute("code"))
		)
	{
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
			//if (XMLUtils::GetString(pProgramNode, "plot", strTmp))
			entry.strPlot = strTmp;

			/* plot outline */
			//if (XMLUtils::GetString(pEpgNode, "plotoutline", strTmp))
			//entry.strPlotOutline = strTmp;
			entry.strPlotOutline = m_strDefaultIcon.c_str();
			entry.strIconPath = m_strDefaultIcon.c_str();

			/* icon path */
			//if (XMLUtils::GetString(pEpgNode, "icon", strTmp))
			if (lpszTmp = pProgramElement->Attribute("picture"))
			{
				entry.strIconPath = lpszTmp;
				entry.strPlotOutline = lpszTmp;
				//XBMC->Log(LOG_DEBUG, "%s - icon path: %s", __FUNCTION__, lpszTmp);
			}

			/* genre type */
			//XMLUtils::GetInt(pEpgNode, "genretype", entry.iGenreType);

			/* genre subtype */
			//XMLUtils::GetInt(pEpgNode, "genresubtype", entry.iGenreSubType);

			//XBMC->Log(LOG_DEBUG, "loaded EPG entry '%s' channel '%d' start '%d' end '%d'",
				//entry.strTitle.c_str(), entry.iChannelId, entry.startTime, entry.endTime);

			epg.push_back(entry);
		}

		int iAddBroadcastId = channel.iChannelNumber << 20;

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

int PVRDemoData::GetRecordingsAmount(void)
{
  return m_recordings.size();
}

PVR_ERROR PVRDemoData::GetRecordings(ADDON_HANDLE handle)
{
  for (std::vector<PVRDemoRecording>::iterator it = m_recordings.begin() ; it != m_recordings.end() ; it++)
  {
    PVRDemoRecording &recording = *it;

    PVR_RECORDING xbmcRecording;

    xbmcRecording.iDuration     = recording.iDuration;
    xbmcRecording.iGenreType    = recording.iGenreType;
    xbmcRecording.iGenreSubType = recording.iGenreSubType;
    xbmcRecording.recordingTime = recording.recordingTime;

    strncpy(xbmcRecording.strChannelName, recording.strChannelName.c_str(), sizeof(xbmcRecording.strChannelName) - 1);
    strncpy(xbmcRecording.strPlotOutline, recording.strPlotOutline.c_str(), sizeof(xbmcRecording.strPlotOutline) - 1);
    strncpy(xbmcRecording.strPlot,        recording.strPlot.c_str(),        sizeof(xbmcRecording.strPlot) - 1);
    strncpy(xbmcRecording.strRecordingId, recording.strRecordingId.c_str(), sizeof(xbmcRecording.strRecordingId) - 1);
    strncpy(xbmcRecording.strTitle,       recording.strTitle.c_str(),       sizeof(xbmcRecording.strTitle) - 1);
    strncpy(xbmcRecording.strStreamURL,   recording.strStreamURL.c_str(),   sizeof(xbmcRecording.strStreamURL) - 1);

    PVR->TransferRecordingEntry(handle, &xbmcRecording);
  }

  return PVR_ERROR_NO_ERROR;
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

	return std::string();
}


#if 0

time_t DateTimeToTimeT(const std::string& datetime)
{
  struct tm timeinfo;
  int year, month ,day;
  int hour, minute, second;
  int count;
  time_t retval;

  count = sscanf(datetime.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);

  if(count != 6)
    return -1;

  timeinfo.tm_hour = hour;
  timeinfo.tm_min = minute;
  timeinfo.tm_sec = second;
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_mday = day;
  // Make the other fields empty:
  timeinfo.tm_isdst = -1;
  timeinfo.tm_wday = 0;
  timeinfo.tm_yday = 0;

  retval = mktime (&timeinfo);

  if(retval < 0)
    retval = 0;

  return retval;
}

#endif // 0