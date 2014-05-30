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

#include "client.h"
#include "xbmc_pvr_dll.h"
#include "PVRDemoData.h"
#include "platform/util/util.h"

#define TARGET_WINDOWS

using namespace std;
using namespace ADDON;

#ifdef TARGET_WINDOWS
#define snprintf _snprintf
#endif

enum EChannelSource
{
  offline = 0,
  online,
  m3u,
};

enum EM3uType
{
  file = 0,
  url,
};

const EChannelSource DEF_CHANNEL_SOURCE = EChannelSource::offline;
const EM3uType DEF_CHANNEL_TYPE = EM3uType::file;
const char* DEF_CA_TEXT           = "";
const char* DEF_M3U_TEXT          = "special://home/addons/pvr.ztv/iptv.m3u";
const char* DEF_MCASTIF           = "255.255.255.255";
const bool  DEF_ENABLE_ONLINE_GRP = true;
const bool  DEF_ENABLE_ONLINE_EPG = true;
const bool  DEF_ENABLE_OFFLINE_CA = false;

bool           m_bCreated       = false;
ADDON_STATUS   m_CurStatus      = ADDON_STATUS_UNKNOWN;
PVRDemoData*   m_data           = NULL;
bool           m_bIsPlaying     = false;
bool           m_bCaSupport     = false;
PVRDemoChannel m_currentChannel;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
string g_strUserPath             = "";
string g_strClientPath           = "";

EChannelSource g_eChannelSource       = DEF_CHANNEL_SOURCE;
EM3uType g_eChannelType               = DEF_CHANNEL_TYPE;
string g_strCaText                    = DEF_CA_TEXT;
string g_strM3uText                   = DEF_M3U_TEXT;
string g_strMCastIf                   = DEF_MCASTIF;
bool g_bEnableOnLineGroups            = DEF_ENABLE_ONLINE_GRP;
bool g_bEnableOnLineEpg               = DEF_ENABLE_ONLINE_EPG;
bool g_bEnableOffLineCa               = DEF_ENABLE_OFFLINE_CA;

CHelper_libXBMC_addon *XBMC           = NULL;
CHelper_libXBMC_pvr   *PVR            = NULL;

extern "C" {

void ADDON_ReadSettings(void)
{
  /* Read setting "host" from settings.xml */
  char buffer[512];

  if (!XBMC)
    return;

  /* Source settings */
  /***********************/
  if (!XBMC->GetSetting("chansource", &g_eChannelSource))
  { 
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'chansource' setting, falling back to 'online' as default");
	g_eChannelSource = DEF_CHANNEL_SOURCE;
  }

  if(EChannelSource::m3u == g_eChannelSource)
  {
	  if (!XBMC->GetSetting("m3utype", &g_eChannelType))
	  { 
		/* If setting is unknown fallback to defaults */
		XBMC->Log(LOG_ERROR, "Couldn't get 'm3utype' setting, falling back to 'file' as default");
		g_eChannelType = DEF_CHANNEL_TYPE;
	  }

	  if(EM3uType::file == g_eChannelType)
	  {
		  /* Read setting "filem3u" from settings.xml */
		  if (XBMC->GetSetting("filem3u", &buffer))
		  { 
			g_strM3uText = buffer;
		  }
		  else
		  {
			/* If setting is unknown fallback to defaults */
			XBMC->Log(LOG_ERROR, "Couldn't get 'filem3u' setting, falling back to 'localhost' as default");
			g_strM3uText = DEF_M3U_TEXT;
		  }
	  }
	  else
	  {
		  /* Read setting "urlm3u" from settings.xml */
		  if (XBMC->GetSetting("urlm3u", &buffer))
		  { 
			g_strM3uText = buffer;
		  }
		  else
		  {
			/* If setting is unknown fallback to defaults */
			XBMC->Log(LOG_ERROR, "Couldn't get 'urlm3u' setting, falling back to 'localhost' as default");
			g_strM3uText = DEF_M3U_TEXT;
		  }
	  }
  }

  /* Read setting "groupenable" from settings.xml */
  if (!XBMC->GetSetting("groupenable", &g_bEnableOnLineGroups))
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'groupenable' setting, falling back to 'true' as default");
	g_bEnableOnLineGroups = DEF_ENABLE_ONLINE_GRP;
  }

  /* Read setting "pin" from settings.xml */
  //if (XBMC->GetSetting("pin", &buffer))
  //{
    //g_szPin = buffer;
  //}
  //else
  //{
    //g_szPin = DEFAULT_PIN;
  //}

  /* Read setting "epgenable" from settings.xml */
  if (!XBMC->GetSetting("epgenable", &g_bEnableOnLineEpg))
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'epgenable' setting, falling back to 'true' as default");
    g_bEnableOnLineEpg = DEF_ENABLE_ONLINE_EPG;
  }

  /* Read setting "caoffline" from settings.xml */
  if (!XBMC->GetSetting("caoffline", &g_bEnableOffLineCa))
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'caoffline' setting, falling back to 'false' as default");
    g_bEnableOnLineEpg = DEF_ENABLE_OFFLINE_CA;
  }

  /* Read setting "ca" from settings.xml */
  if (XBMC->GetSetting("ca", &buffer))
  { 
    g_strCaText = buffer;
  }
  else
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'ca' setting, falling back to '' as default");
	g_strCaText = DEF_CA_TEXT;
  }

  /* Read setting "mcastif" from settings.xml */
  if (XBMC->GetSetting("mcastif", &buffer))//&g_ulMCastIf))
  {
	g_strMCastIf = buffer;
  }
  else
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'mcastif' setting, falling back to '255.255.255.255' as default");
    g_strMCastIf = DEF_MCASTIF;
  }

  /* Log the current settings for debugging purposes */
  XBMC->Log(LOG_DEBUG, "settings: chansource='%u', epgenable=%u", g_eChannelSource, g_bEnableOnLineEpg);
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!hdl || !props)
    return ADDON_STATUS_UNKNOWN;

  PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*)props;

  XBMC = new CHelper_libXBMC_addon;
  if (!XBMC->RegisterMe(hdl))
  {
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  PVR = new CHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  XBMC->Log(LOG_DEBUG, "%s - Creating the PVR ztv add-on", __FUNCTION__);

  m_CurStatus     = ADDON_STATUS_UNKNOWN;
  g_strUserPath   = pvrprops->strUserPath;
  g_strClientPath = pvrprops->strClientPath;

  ADDON_ReadSettings();

  m_data = new PVRDemoData(g_bEnableOnLineEpg, g_strMCastIf.c_str());
  bool bIsOnLine = (EChannelSource::online == g_eChannelSource);
  if(bIsOnLine || g_bEnableOffLineCa)
  {
	m_bCaSupport = m_data->VLCInit(g_strCaText.c_str());

	if(!m_bCaSupport)
	{
		m_CurStatus = ADDON_STATUS_LOST_CONNECTION;
	}
  }

  if(ADDON_STATUS_UNKNOWN == m_CurStatus)
  {
	  string strM3uPath;
	  if(EChannelSource::m3u == g_eChannelSource)
	  {
		  strM3uPath = g_strM3uText;
	  }
	  bool bSuccess = m_data->LoadChannelsData(strM3uPath, bIsOnLine, g_bEnableOnLineGroups);
      m_CurStatus = (bSuccess)?ADDON_STATUS_OK:ADDON_STATUS_LOST_CONNECTION;
  }

  m_bCreated = true;

  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  return m_CurStatus;
}

void ADDON_Destroy()
{
  delete m_data;
  m_bCreated = false;
  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

bool ADDON_HasSettings()
{
  return true;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{  
  return 0;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  string str = settingName;

  // SetSetting can occur when the addon is enabled, but TV support still
  // disabled. In that case the addon is not loaded, so we should not try
  // to change its settings.
  if (!XBMC)
    return ADDON_STATUS_OK;

  if (str == "chansource")
  {
    if (g_eChannelSource != *(EChannelSource*) settingValue)
    {
      XBMC->Log(LOG_INFO, "Changed setting 'chansource' from %u to %u", g_eChannelSource, *(int*) settingValue);
      g_eChannelSource = *(EChannelSource*) settingValue;
    }
	else
	{
	  return ADDON_STATUS_OK;
	}
  }
  else if (str == "groupenable")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'groupenable' from %u to %u", g_bEnableOnLineGroups, *(bool*) settingValue);
    g_bEnableOnLineGroups = *(bool*) settingValue;
  }
  else if (str == "epgenable")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'epgenable' from %u to %u", g_bEnableOnLineEpg, *(bool*) settingValue);
    g_bEnableOnLineEpg = *(bool*) settingValue;
  }
  else if (str == "caoffline")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'caoffline' from %u to %u", g_bEnableOffLineCa, *(bool*) settingValue);
    g_bEnableOffLineCa = *(bool*) settingValue;
  }
  else if (str == "ca")
  {
    XBMC->Log(LOG_INFO, "Changed setting 'ca' from '%s' to '%s'", g_strCaText.c_str(), (const char*) settingValue);
    g_strCaText = (const char*) settingValue;
  }
  else if (str == "mcastif")
  {
	  XBMC->Log(LOG_INFO, "Changed setting 'mcastif' from %s to %s", g_strMCastIf.c_str(), (const char*) settingValue);
    g_strMCastIf = (const char*) settingValue;
  }

  return ADDON_STATUS_NEED_RESTART;
}

void ADDON_Stop()
{
  if(m_bCaSupport)
  {
		m_data->FreeVLC();
  }
}

void ADDON_FreeSettings()
{
}

//void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
//{
//}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

const char* GetPVRAPIVersion(void)
{
  static const char *strApiVersion = XBMC_PVR_API_VERSION;
  return strApiVersion;
}

const char* GetMininumPVRAPIVersion(void)
{
  static const char *strMinApiVersion = XBMC_PVR_MIN_API_VERSION;
  return strMinApiVersion;
}

//const char* GetGUIAPIVersion(void)
//{
  //static const char *strGuiApiVersion = XBMC_GUI_API_VERSION;
  //return strGuiApiVersion;
//}

//const char* GetMininumGUIAPIVersion(void)
//{
  //static const char *strMinGuiApiVersion = XBMC_GUI_MIN_API_VERSION;
  //return strMinGuiApiVersion;
//}


PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG             = true;
  pCapabilities->bSupportsTV              = true;
  pCapabilities->bSupportsRadio           = true;
  pCapabilities->bSupportsChannelGroups   = true;
  pCapabilities->bSupportsRecordings      = false;
  pCapabilities->bHandlesInputStream      = true;

  return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{
  static const char *strBackendName = "ViPetroFF ztv pvr add-on";
  return strBackendName;
}

const char *GetBackendVersion(void)
{
  static CStdString strBackendVersion = "0.1";
  return strBackendVersion.c_str();
}

const char *GetConnectionString(void)
{
  static CStdString strConnectionString = "connected";
  return strConnectionString.c_str();
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{
  *iTotal = 1024 * 1024 * 1024;
  *iUsed  = 0;
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  if (m_data)
    return m_data->GetEPGForChannel(handle, channel, iStart, iEnd);

  return PVR_ERROR_SERVER_ERROR;
}

int GetChannelsAmount(void)
{
  if (m_data)
    return m_data->GetChannelsAmount();

  return -1;
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  if (m_data)
    return m_data->GetChannels(handle, bRadio);

  return PVR_ERROR_SERVER_ERROR;
}

#if 0
bool OpenLiveStream(const PVR_CHANNEL &channel)
{
  if (m_data)
  {
    CloseLiveStream();

    if (m_data->GetChannel(channel, m_currentChannel))
    {
      m_bIsPlaying = true;
      return true;
    }
  }

  return false;
}

void CloseLiveStream(void)
{
  m_bIsPlaying = false;
}

bool SwitchChannel(const PVR_CHANNEL &channel)
{
  CloseLiveStream();

  return OpenLiveStream(channel);
}

int GetCurrentClientChannel(void)
{
  return m_currentChannel.iUniqueId;
}
#endif // 0

/*******************************************/
/** PVR Live Stream Functions             **/

bool OpenLiveStream(const PVR_CHANNEL &channelinfo)
{
  if (!m_data)
    return false;
  else
    return m_data->OpenLiveStream(channelinfo);
}

void CloseLiveStream()
{
  if (m_data)
    m_data->CloseLiveStream();
}

bool SwitchChannel(const PVR_CHANNEL &channelinfo)
{
  if (!m_data)
    return false;
  else
    return m_data->SwitchChannel(channelinfo);
}

int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize)
{
  if (!m_data)
    return 0;
  else
    return m_data->ReadLiveStream(pBuffer, iBufferSize);
}

int GetCurrentClientChannel()
{
  if (!m_data)
    return 0;
  else
    return m_data->GetCurrentClientChannel();
}

const char * GetLiveStreamURL(const PVR_CHANNEL &channel)
{
  //if (!m_data)
    //return "";
  //else
    //return m_data->GetLiveStreamURL(channel);

  return NULL;
}

bool CanPauseStream(void)
{
  if (!m_data)
    return false;
  else
    return m_data->CanPauseStream();
}

void PauseStream(bool bPaused)
{
}

PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties)
{
  return PVR_ERROR_NOT_IMPLEMENTED;
}

int GetChannelGroupsAmount(void)
{
  if (m_data)
    return m_data->GetChannelGroupsAmount();

  return -1;
}

PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  if (m_data)
    return m_data->GetChannelGroups(handle, bRadio);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  if (m_data)
    return m_data->GetChannelGroupMembers(handle, group);

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus)
{
  snprintf(signalStatus.strAdapterName, sizeof(signalStatus.strAdapterName), "pvr ztv iptv adapter 1");
  snprintf(signalStatus.strAdapterStatus, sizeof(signalStatus.strAdapterStatus), "OK");

  return PVR_ERROR_NO_ERROR;
}

int GetRecordingsAmount(void)
{
  if (m_data)
    return m_data->GetRecordingsAmount();

  return -1;
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle)
{
  if (m_data)
    return m_data->GetRecordings(handle);

  return PVR_ERROR_NOT_IMPLEMENTED;
}

/** UNUSED API FUNCTIONS */
PVR_ERROR DialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DialogAddChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool OpenRecordedStream(const PVR_RECORDING &recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */) { return 0; }
long long PositionRecordedStream(void) { return -1; }
long long LengthRecordedStream(void) { return 0; }
void DemuxReset(void) {}
void DemuxFlush(void) {}
//int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */) { return -1; }
long long PositionLiveStream(void) { return -1; }
long long LengthLiveStream(void) { return -1; }
//const char * GetLiveStreamURL(const PVR_CHANNEL &channel) { return ""; }
PVR_ERROR DeleteRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) { return -1; }
//PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
int GetTimersAmount(void) { return -1; }
PVR_ERROR GetTimers(ADDON_HANDLE handle) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR AddTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
void DemuxAbort(void) {}
DemuxPacket* DemuxRead(void) { return NULL; }
unsigned int GetChannelSwitchDelay(void) { return 0; }
//void PauseStream(bool bPaused) {}
//bool CanPauseStream(void) { return false; }
bool CanSeekStream(void) { return false; }
bool SeekTime(int,bool,double*) { return false; }
void SetSpeed(int) {};
}
