#pragma once
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

#include <map>
#include <hash_map>
#include <vector>
#include "platform/util/StdString.h"
#include "platform/threads/mutex.h"
#include "client.h"

/*!
 * @brief PVR macros for string exchange
 */
#define PVR_STRCPY(dest, source) do { strncpy(dest, source, sizeof(dest)-1); dest[sizeof(dest)-1] = '\0'; } while(0)
#define PVR_STRCLR(dest) memset(dest, 0, sizeof(dest))

enum EChannelsSort
{
  none = 0,
  id,
  name,
  ip,
  uri,
};

struct PVRDemoEpgEntry
{
  int         iBroadcastId;
  int         iChannelId;
  int         iGenreType;
  int         iGenreSubType;
  time_t      startTime;
  time_t      endTime;
  std::string strTitle;
  std::string strPlotOutline;
  std::string strPlot;
  std::string strIconPath;
};

struct PVRDemoChannel
{
  bool                    bRadio;
  int                     iUniqueId;
  int                     iChannelNumber;
  int                     iGroupId;
  unsigned long           ulIpNumber;
  bool                    bIsTcpTransport;
  int                     iEncryptionSystem;
  std::string             strChannelName;
  std::string             strIconPath;
  std::string             strStreamURL;

  bool operator<(const PVRDemoChannel& other) const { return (iChannelNumber < other.iChannelNumber); }
  bool operator==(const PVRDemoChannel& other) const { return (iUniqueId == other.iUniqueId); }
};

struct PVRDemoChannelGroup
{
  bool             bRadio;
  int              iGroupId;
  std::string      strGroupName;
  std::vector<int> members;
};

namespace LibVLCCAPlugin
{
class ILibVLCModule;
} // LibVLCCAPlugin

class PVRDemoData
{
public:
  PVRDemoData(bool bIsEnableOnLineEpg, LPCSTR lpszMCastIf);
  virtual ~PVRDemoData(void);

  virtual bool VLCInit(LPCSTR lpszCA);
  virtual void FreeVLC(void);
  virtual void ProxyAddrInit(LPCSTR lpszIP, int iPort, bool bCaSupport);
  virtual bool LoadChannelsData(const std::string& strM3uPath, bool bIsOnLineSource, bool bIsEnableOnLineGroups, EChannelsSort sortby);
  virtual int GetChannelsAmount(void);
  virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
  virtual bool GetChannel(const PVR_CHANNEL &channel, PVRDemoChannel &myChannel);

  virtual int GetChannelGroupsAmount(void);
  virtual PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio);
  virtual PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);

  virtual PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);
  virtual PVR_ERROR RequestWebEPGForChannel(ADDON_HANDLE handle, const PVRDemoChannel& channel, time_t iStart, time_t iEnd);

  virtual bool OpenLiveStream(const PVR_CHANNEL &channelinfo);
  virtual void CloseLiveStream();
  virtual bool SwitchChannel(const PVR_CHANNEL &channelinfo);
  virtual int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize);
  virtual int GetCurrentClientChannel();
  virtual const char * GetLiveStreamURL(const PVR_CHANNEL &channel);
  virtual bool CanPauseStream();
  void PauseStream(bool bPaused);
  bool CanSeekStream();

  virtual std::string GetIconPath(LPCSTR lpszIcoFName) const;
protected:
  virtual bool LoadWebXmlData(const std::string& strMac, bool bIsEnableOnLineGroups);
  virtual bool LoadWebXmlGroups();
  virtual bool LoadWebXmlChannels(const std::string& strMac);
  virtual bool LoadM3UList(const std::string& strM3uUri);
  int DoHttpRequest(const CStdString& resource, const CStdString& body, CStdString& response);
  CStdString PVRDemoData::ReadMarkerValue(std::string &strLine, const char* strMarkerName);
  int GetChannelId(const char * strStreamUrl, unsigned int uiChannelId);
private:
  static const char* ZTV_CASERVER_URI;
  static const char* ZTV_EPGSERVER_URI;

  std::map<std::wstring, int>      m_mapLogo;
  std::vector<PVRDemoChannelGroup> m_groups;
  std::vector<PVRDemoChannel>      m_channels;
  time_t                           m_iEpgStart;
  CStdString                       m_strDefaultIcon;
  CStdString                       m_strDefaultMovie;
  CStdString                       m_strProxyAddr;
  PLATFORM::CMutex                 m_mutex;
  LibVLCCAPlugin::ILibVLCModule*   m_ptrVLCCAModule;
  IStream*                         m_currentStream;
  PVRDemoChannel                   m_currentChannel;
  ULONG                            m_ulMCastIf;
  bool                             m_bIsEnableOnLineEpg;
  bool                             m_bCaSupport;
};
