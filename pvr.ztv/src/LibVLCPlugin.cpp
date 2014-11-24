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

#include <ws2tcpip.h>
#include <Iphlpapi.h>
#include <iostream>

#include "comstream.h"
#include "LibVLCPlugin.h"

using namespace std;
using namespace ADDON;

namespace LibVLCCAPlugin 
{

static LONG __declspec(naked) WINAPI InvokeFunc(PVOID funcptr)
{
__asm
{
	pop	ecx	//; save return address
	pop	edx	//; Get function pointer
	push	ecx	//; Restore return address
	jmp	edx	//; Transfer control to the function pointer}
}
}

static LONG __declspec(naked) WINAPI InvokePluginInit(PVOID funcptr)
{
__asm
{
	pop	ecx	//; save return address
	pop	edx	//; Get function pointer
	pop	eax	//; Get string pointer
	push ecx //; Restore return address
	jmp	edx	//; Transfer control to the function pointer
}
}

static LONG (WINAPI *InvokePluginInitStr)(PVOID funcptr, LPCSTR strParam) =
	reinterpret_cast<LONG (WINAPI *)(PVOID funcptr, LPCSTR strParam)>(InvokePluginInit);
static LONG (WINAPI *InvokeFuncPvoid)(PVOID funcptr, PVOID pAccess) =
	reinterpret_cast<LONG (WINAPI *)(PVOID funcptr, PVOID pAccess)>(InvokeFunc);
static LONG (WINAPI *InvokeFuncPvoidPbyteDword)(PVOID funcptr, PBYTE pBuffer, DWORD dwSize) =
	reinterpret_cast<LONG (WINAPI *)(PVOID funcptr, PBYTE pBuffer, DWORD dwSize)>(InvokeFunc);
static LONG (WINAPI *InvokeFuncPvoidStrStrStr)(PVOID funcptr, PVOID pLibVLC, LPCSTR strAccess, LPCSTR strDemux, LPCSTR strPath) =
	reinterpret_cast<LONG (WINAPI *)(PVOID funcptr, PVOID pLibVLC, LPCSTR strAccess, LPCSTR strDemux, LPCSTR strPath)>(InvokeFunc);
static LONG (WINAPI *InvokeFuncStrWord)(PVOID funcptr, LPCSTR strIp, WORD wPort) =
	reinterpret_cast<LONG (WINAPI *)(PVOID funcptr, LPCSTR strIp, WORD wPort)>(InvokeFunc);


void Tokenize(const string& str, vector<string>& tokens, const string& delimiters = " ")
{
  string::size_type start_pos = 0;
  string::size_type delim_pos = 0;

  while (string::npos != delim_pos)
  {
    delim_pos = str.find_first_of(delimiters, start_pos);
    tokens.push_back(str.substr(start_pos, delim_pos - start_pos));
    start_pos = delim_pos + 1;
    // Find next "non-delimiter"
  }
}

class CLibVLCAccess;

class CLibVLCModule: public ILibVLCModule
{
public:
	CLibVLCModule(const CStdString& cstrConfigPath, const CStdString& cstrCaUri, ULONG ulMCastIPAddr);
    CLibVLCModule(const CStdString& cstrInitParamCAPlugin, const CStdStringArray& args)
    {
        LibVLCModuleInit(cstrInitParamCAPlugin, args, NULL, -1, 0);
    }

	virtual ~CLibVLCModule()
	{
		LibVLCRelease();
	}

	void LibVLCRelease();

	virtual IStream* NewAccess(const CStdString& strMrl);
	virtual CStdString GetBestMacAddress();

protected:
private:
	struct libvlc_exception_t
	{
		int b_raised;
		int i_code;
		char *psz_message;
	};

	typedef struct
	{
		LONG initialized;
		CRITICAL_SECTION mutex;
	} vlc_mutex_t;

	struct libvlc_instance_t
	{
		HANDLE        p_libvlc_int; // libvlc_int_t *p_libvlc_int;
		HANDLE        p_vlm; // vlm_t        *p_vlm;
		int           b_playlist_locked;
		unsigned      ref_count;
		int           verbosity;
		vlc_mutex_t   instance_lock;
		vlc_mutex_t   event_callback_lock;
		PVOID         p_callback_list; // struct libvlc_callback_entry_list_t *p_callback_list;
	};

    static const CStdString _cstrCAPluginZetFileName;
    static const CStdString _cstrCALibVLCHookFileName;
	static const CStdString _cstrLibVLCFileName;
	static const CStdString _cstrLibVLCCoreFileName;
    static const CStdString _cstrInitZetIpTvPlugin;
    static const CStdString _cLibVLCArgs[];

	CStdString _strConfigPath;
	CStdString _strCaHost;
    HANDLE _hmodCAPluginZet;
    HANDLE _hmodCALibVLCHook;
	HANDLE _hmodLibVLC;
	HANDLE _hmodVLCCore;
    libvlc_instance_t* _pLibVLCHandle;
    CLibVLCAccess* _pLibAccess;
	DWORD (WINAPI *_GetBestMacAddress)(PUCHAR uchPhysAddr, DWORD dwSize);

	void (*libvlc_exception_init)(libvlc_exception_t* pExp);
	int (*libvlc_exception_raised)(libvlc_exception_t* pExp);
	libvlc_instance_t* (*libvlc_new)(int nArgc, const char *const * Args, libvlc_exception_t* pExp);
	void (*libvlc_release)(libvlc_instance_t* plibVLC);

    HANDLE GetLibVLCObject() const
    {
		return _pLibVLCHandle->p_libvlc_int;
    }

    void LibVLCModuleInit(const CStdString& cstrInitParamCAPlugin, const CStdStringArray& args, LPCSTR lpszCAMac, ULONG ulMCastIPAddr, int iCAPort);
	bool CALibVLCHookLoad();
	void CAPluginZetInit(const CStdString&  cstrInitParam);
	void CALibVLCHookInit(LPCSTR lpszCAMac, ULONG ulMCastIPAddr, int iCAPort);
	void LibVLCInit(const CStdStringArray& args);
	void LibVLCCoreInit();
};


class CLibVLCAccess
{
public:
	struct access_t;
	struct block_t;

    struct access_t
    {
        LPCSTR psz_object_type;
        PCHAR psz_object_name;

        PCHAR psz_header;
        int i_flags;

        //volatile bool b_error;
        //volatile bool b_die;
        //bool b_force;
        //bool be_sure_to_add_VLC_COMMON_MEMBERS_to_struct;
        unsigned int boolOptions;

        PVOID p_libvlc;
        PVOID p_parent;
        PVOID p_private;

        // access structure
        PVOID p_module;
        PCHAR psz_access;
        PCHAR psz_path;
        PCHAR psz_demux;

        //ssize_t     (*pf_read) ( access_t *, uint8_t *, size_t );   Return -1 if no data yet, 0 if no more data, else real data read
        PVOID pf_read;
        //block_t    *(*pf_block)( access_t * );                   return a block of data in his 'natural' size, NULL if not yet data or eof
        //PVOID pf_block;
		block_t    *(*pf_block)( access_t * );
        //int         (*pf_seek) ( access_t *, int64_t );         can be null if can't seek
        PVOID pf_seek;
        //int         (*pf_control)( access_t *, int i_query, va_list args);
        PVOID pf_control;

        // Access has to maintain them uptodate
        //struct
        //{
        //unsigned int i_update;  Access sets them on change, Input removes them once take into account

        LONGLONG i_size;//  Write only for access, read only for input
        LONGLONG i_pos; //idem
        //bool         b_eof; idem
        int b_eof; // idem

        int i_title;     //idem, start from 0 (could be menu)
        int i_seekpoint; //idem, start from 0
        //} info;
        PVOID p_sys;

        static int SysOffset()
        {
			return 26;
        }
    };

	typedef void (*block_free_t) (block_t *);

    struct block_t
    {
        block_t* p_next;
        UINT i_flags;

        LONGLONG i_pts;
        LONGLONG i_dts;
        LONGLONG i_length;

        int i_samples; // Used for audio
        int i_rate;

        int i_buffer;
        PBYTE p_buffer;

        // Rudimentary support for overloading block (de)allocation.
        block_free_t pf_release;
    };

    class MRLParts
    {
	public:
        const CStdString& Access()
        {
                return _strAccess;
        }

        const CStdString& Demux()
        {
                return _strDemux;
        }

        const CStdString& Path()
        {
                return _strPath;
        }

        MRLParts(const CStdString& strMrl)
        {
			ParseMRL(strMrl);
		}

        MRLParts(const CStdString& strAccess, const CStdString& strDemux, const CStdString& strPath)
        {
            _strAccess = strAccess;
            _strDemux = strDemux;
            _strPath = strPath;
        }
	protected:
        bool ParseMRL(const CStdString& strMrl)
        {
            bool result = false;

			int ndx = strMrl.Find("://");
            if (ndx >= 0)
            {
				CStdString strAccess = strMrl.Mid(0, ndx);
                CStdString strPath = strMrl.Mid(ndx + 3);
                vector<string> arrAccessDemux;
				Tokenize(strAccess, arrAccessDemux, "/");

                CStdString strDemux;
                ndx = 0;
				if (ndx < arrAccessDemux.size())
                {
                    strAccess = arrAccessDemux[ndx++];
                }
				if (ndx < arrAccessDemux.size())
                {
                    strDemux = arrAccessDemux[ndx++];
                }

				_strAccess = strAccess;
				_strDemux = strDemux;
				_strPath = strPath;

                result = true;
            }
            else
            {
                _strPath = strMrl;
            }

            return result;
        }
	private:
        CStdString _strAccess, _strDemux, _strPath;

    };

	typedef access_t* (* FAccessNew)(HANDLE p_obj, const char *psz_access, const char *psz_demux, const char *psz_path);
	typedef void (* FAccessDelete)(access_t* p_access);

    CLibVLCAccess(HANDLE LibVLCObject, PVOID funcAccessNew, PVOID funcAccessDelete)
    {
        if (NULL == LibVLCObject || NULL == funcAccessNew || NULL == funcAccessDelete)
        {
            throw invalid_argument("LibVLCObject, funcAccessNew, funcAccessDelete");
        }

        _LibVLCObject = LibVLCObject;
        _funcAccessNew = reinterpret_cast<FAccessNew>(funcAccessNew);
        _funcAccessDelete = reinterpret_cast<FAccessDelete>(funcAccessDelete);
    }

    IStream* OpenMRL(string strMrl);

protected:
private:

    HANDLE _LibVLCObject;
	FAccessNew _funcAccessNew;
	FAccessDelete _funcAccessDelete;
};

ILibVLCModule* ILibVLCModule::NewModule(const CStdString& cstrConfigPath, const CStdString& cstrCaUri, ULONG ulMCastIPAddr)
{
	CLibVLCModule* pmodule = NULL;

	try
	{
		pmodule = new CLibVLCModule(cstrConfigPath, cstrCaUri, ulMCastIPAddr);

		in_addr in={0};
		in.s_addr=ulMCastIPAddr;

		XBMC->Log(LOG_DEBUG, "Initialize vlc module: cfg path='%s', ca-uri='%s', mcast-ip='%s'", cstrConfigPath.c_str(), cstrCaUri.c_str(), inet_ntoa(in));
		//XBMC->Log(LOG_DEBUG, "user cfg path='%s'", g_strUserPath.c_str());
		//XBMC->Log(LOG_DEBUG, "Initialize vlc module");
		//CStdString strMac = pmodule->GetBestMacAddress();
		//XBMC->Log(LOG_DEBUG, "Best mac address: %s", strMac.c_str());
	}
	catch(const exception& excp)
	{
		XBMC->Log(LOG_ERROR, "Vlc module initialization failed, error: %s", excp.what());
	}

	return pmodule;
}

const CStdString CLibVLCModule::_cstrCAPluginZetFileName = "IpTvPvr.Plugin.InterZet.dll";
const CStdString CLibVLCModule::_cstrCALibVLCHookFileName = "LibVLCHook.dll";
const CStdString CLibVLCModule::_cstrLibVLCFileName = "LibVlc.dll";
const CStdString CLibVLCModule::_cstrLibVLCCoreFileName = "libvlccore.dll";
const CStdString CLibVLCModule::_cstrInitZetIpTvPlugin = " frmPar=\"99999999\" ver=\"0.28.1.8823\""\
    " cfUser=\"${CFG_PATH}IpTvPvr.User.ini\""\
    " cfVlc=\"${CFG_PATH}IpTvPvr.Vlc.ini\""\
    " cfProv=\"${CFG_PATH}Provider.ini\""\
    " HttpCmdFunc=\"4301632\" ";
const CStdString CLibVLCModule::_cLibVLCArgs[] = {
                                    "--config=${CFG_PATH}.IpTvPvr.Vlc.ini",
                                    //"-vvv",
                                    "--plugin-path=${CFG_PATH}plugins",
									"--ignore-config",
                                    "--no-plugins-cache",
                                    "--no-osd",
                                    "--no-media-library",
                                    "--no-one-instance",
									"--miface-addr="
                                };


CLibVLCModule::CLibVLCModule(const CStdString& cstrConfigPath, const CStdString& cstrCaUri, ULONG ulMCastIPAddr)
{
    _hmodCAPluginZet = NULL;
    _hmodCALibVLCHook = NULL;
	_hmodLibVLC = NULL;
	_hmodVLCCore = NULL;
    _pLibVLCHandle = NULL;
    _pLibAccess = NULL;
	_GetBestMacAddress = NULL;

	CStdString strConfigPath(cstrConfigPath);

	if ('\\' != strConfigPath.back())
	{
		strConfigPath += '\\';
	}

	_strConfigPath = strConfigPath;

	CStdStringArray arrLibVLCArgs;
	
	arrLibVLCArgs.assign(_cLibVLCArgs, _cLibVLCArgs + _countof(_cLibVLCArgs));
	arrLibVLCArgs.front().Replace("${CFG_PATH}", strConfigPath);
	(arrLibVLCArgs.begin()+1)->Replace("${CFG_PATH}", strConfigPath);
	if (~ulMCastIPAddr && ulMCastIPAddr)
	{
		in_addr in={0};
		in.S_un.S_addr=ulMCastIPAddr;
		arrLibVLCArgs.back() =+ inet_ntoa(in);
	}
	else
	{
		arrLibVLCArgs.pop_back();
	}

	LPCSTR lpszCAMac = NULL;
	int iCAPort = 0;
	if (!cstrCaUri.IsEmpty())
	{
		int ndx = cstrCaUri.Find("://");
		if (ndx > 0)
		{
			_strCaHost = cstrCaUri.Mid(ndx + 3);
			ndx = cstrCaUri.Find(':', ndx+1);
			if (ndx > 0)
			{
				int ndxx = cstrCaUri.Find('/', ndx);
				if(ndxx > 0)
				{
					ndx++;
					iCAPort = atoi(cstrCaUri.Mid(ndx, ndxx-ndx));
				}
			}

			ndx = _strCaHost.Find('/');
			if(ndx > -1)
			{
				_strCaHost = _strCaHost.Left(ndx);
			}
			ndx = _strCaHost.Find(':');
			if(ndx > -1)
			{
				_strCaHost = _strCaHost.Left(ndx);
			}

			arrLibVLCArgs.push_back("--ca-authuri=" + cstrCaUri);
		}
		else
		{
			lpszCAMac = cstrCaUri.c_str();
			_strCaHost = "watch-tv.zet";
			arrLibVLCArgs.push_back("--ca-authuri=https://watch-tv.zet/ca/");
		}
	}
	else
	{
		_strCaHost = "watch-tv.zet";
		arrLibVLCArgs.push_back("--ca-authuri=https://watch-tv.zet/ca/");
	}

	CStdString strInitZetIpTvPlugin = _cstrInitZetIpTvPlugin;

	strInitZetIpTvPlugin.Replace("${CFG_PATH}", strConfigPath);
	LibVLCModuleInit(strInitZetIpTvPlugin, arrLibVLCArgs, lpszCAMac, ulMCastIPAddr, iCAPort);
}

void CLibVLCModule::LibVLCModuleInit(const CStdString& cstrInitParamCAPlugin, const CStdStringArray& args, LPCSTR lpszCAMac, ULONG ulMCastIPAddr, int iCAPort)
{
    try
    {
        bool bSuccess = CALibVLCHookLoad();
        CAPluginZetInit(cstrInitParamCAPlugin);
        if (bSuccess)
        {
            CALibVLCHookInit(lpszCAMac, ulMCastIPAddr, iCAPort);
        }
        LibVLCInit(args);
        LibVLCCoreInit();
    }
    catch (const exception& ex)
    {
        LibVLCRelease();
        throw ex;
    }
}

void CLibVLCModule::LibVLCRelease()
{
	if(NULL != _pLibAccess)
	{
		delete _pLibAccess;
		_pLibAccess = NULL;
	}
	//XBMC->Log(LOG_DEBUG, "libvlc_release(????");
    if (NULL != _pLibVLCHandle)
    {
		//libvlc_release(_pLibVLCHandle);
        _pLibVLCHandle = NULL;
    }
	//XBMC->Log(LOG_DEBUG, "libvlc_release(!!!!!");
    if (NULL != _hmodVLCCore)
    {
        dlclose(_hmodVLCCore);
        _hmodVLCCore = NULL;
    }

    if (NULL != _hmodLibVLC)
    {
        dlclose(_hmodLibVLC);
        _hmodLibVLC = NULL;
    }

    if (NULL != _hmodCALibVLCHook)
    {
        dlclose(_hmodCALibVLCHook);
        _hmodCAPluginZet = NULL;
    }

    if (NULL != _hmodCAPluginZet)
    {
        dlclose(_hmodCAPluginZet);
        _hmodCAPluginZet = NULL;
    }
}


void CLibVLCModule::CAPluginZetInit(const CStdString&  cstrInitParam)
{
	CStdString strCAPluginZetFilePath = _strConfigPath + _cstrCAPluginZetFileName;

	if (XBMC->FileExists(strCAPluginZetFilePath, false))
	{
		//XBMC->Log(LOG_DEBUG, "%s - path: %s", __FUNCTION__, strCAPluginZetFilePath.c_str());
		HANDLE hmodCAPluginZet = dlopen(strCAPluginZetFilePath, RTLD_LAZY);
        if (NULL != hmodCAPluginZet)
        {
            PVOID funcaddr = dlsym(hmodCAPluginZet, "IpTvPlayerPluginInit");
            if (0 != funcaddr)
            {
				//XBMC->Log(LOG_DEBUG, "%s - 0x%x", __FUNCTION__, funcaddr);
				InvokePluginInitStr(funcaddr, cstrInitParam);
                _hmodCAPluginZet = hmodCAPluginZet;
            }
            else
            {
				system_error err(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
				dlclose(hmodCAPluginZet);
				throw err;
            }
        }
        else
        {
			throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
        }
    }
}

bool CLibVLCModule::CALibVLCHookLoad()
{
    bool IsLoaded = false;
	CStdString strCALibVLCHookFilePath = _strConfigPath + _cstrCALibVLCHookFileName;
    
	if (XBMC->FileExists(strCALibVLCHookFilePath, false))
    {
		HANDLE hmodCALibVLCHook = dlopen(strCALibVLCHookFilePath, RTLD_LAZY);
		if (NULL != hmodCALibVLCHook)
        {
			PVOID funcaddr = dlsym(hmodCALibVLCHook, "_CAServerAddressInit@8");
            if (0 != funcaddr)
            {
                funcaddr = dlsym(hmodCALibVLCHook, "_GetBestMacAddress@8");
            }

            if (0 != funcaddr)
            {
                _hmodCALibVLCHook = hmodCALibVLCHook;
                IsLoaded = true;
            }
            else
            {
				system_error err(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
				dlclose(hmodCALibVLCHook);
				throw err;
            }
        }
        else
        {
			throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
        }
    }

    return IsLoaded;
}

void CLibVLCModule::CALibVLCHookInit(LPCSTR lpszCAMac, ULONG ulMCastIPAddr, int iCAPort)
{
    if (NULL != _hmodCALibVLCHook)
    {
		HRESULT (WINAPI *funcaddrInit)(LPCSTR lpszIP, WORD wPort)=
			reinterpret_cast<HRESULT (WINAPI *)(LPCSTR lpszIP, WORD wPort)>(dlsym(_hmodCALibVLCHook, "_CAServerAddressInit@8"));

		if (NULL != funcaddrInit)
        {
			PVOID funcaddrGet = dlsym(_hmodCALibVLCHook, "_GetBestMacAddress@8");
			if (NULL != funcaddrGet)
			{
				CStdString strMCastIPAddr;
				LPCSTR szMCastIPAddr=NULL;
			
				if(lpszCAMac && *lpszCAMac)
				{
					szMCastIPAddr = lpszCAMac;
				}
				else if (~ulMCastIPAddr && ulMCastIPAddr)
				{
					in_addr in={0};
					in.S_un.S_addr=ulMCastIPAddr;
					strMCastIPAddr = inet_ntoa(in);
					szMCastIPAddr = strMCastIPAddr.c_str();
				}

				HRESULT status = funcaddrInit(szMCastIPAddr, static_cast<WORD>(iCAPort));
				if (FAILED(status))
				{
					//throw new Win32Exception((int)status);
					SetLastError(status);
					throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
				}
				_GetBestMacAddress = reinterpret_cast<DWORD (WINAPI *)(PUCHAR uchPhysAddr, DWORD dwSize)>(funcaddrGet);
			}
			else
			{
				//int iErrCode = Marshal.GetLastWin32Error();
				//throw new Win32Exception(Marshal.GetLastWin32Error());
				throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
			}
        }
        else
        {
            //int iErrCode = Marshal.GetLastWin32Error();
            //throw new Win32Exception(Marshal.GetLastWin32Error());
			throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
        }
    }
    else
    {
        //throw new Win32Exception((int)1157L);
		SetLastError(1157L);
		throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
    }
}

void CLibVLCModule::LibVLCInit(const CStdStringArray& args)
{
	CStdString strLibVLCFilePath = _strConfigPath + _cstrLibVLCFileName;
	_hmodLibVLC = dlopen(strLibVLCFilePath, RTLD_LAZY);
	if (NULL != _hmodLibVLC)
	{
		libvlc_exception_init = reinterpret_cast<void (*)(libvlc_exception_t* pExp)>(dlsym(_hmodLibVLC, "libvlc_exception_init"));
		if (NULL == libvlc_exception_init)
		{
			throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
		}
		libvlc_exception_raised = reinterpret_cast<int (*)(libvlc_exception_t* pExp)>(dlsym(_hmodLibVLC, "libvlc_exception_raised"));
		if (NULL == libvlc_exception_raised)
		{
			throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
		}
		libvlc_new = reinterpret_cast<libvlc_instance_t* (*)(int nArgc, const char *const * Args, libvlc_exception_t* pExp)>(dlsym(_hmodLibVLC, "libvlc_new"));
		if (NULL == libvlc_new)
		{
			throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
		}
		libvlc_release = reinterpret_cast<void (*)(libvlc_instance_t* plibVLC)>(dlsym(_hmodLibVLC, "libvlc_release"));
		if (NULL == libvlc_release)
		{
			throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
		}
	}
	else
	{
		throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
	}

	libvlc_exception_t exp={0};
    libvlc_exception_init(&exp);

	LPCSTR arrArgs[16]={0};
	LPCSTR* arrArgsPos=arrArgs;
	for(CStdStringArray::const_iterator pos=args.begin(); pos != args.end(); pos++)
	{
		*arrArgsPos=pos->c_str();
		//XBMC->Log(LOG_DEBUG, "%s - arrArgsPos: %s", __FUNCTION__, *arrArgsPos);
		arrArgsPos++;
	}

	libvlc_instance_t* pLibVLC = libvlc_new(args.size(), arrArgs, &exp);
    if (0 != libvlc_exception_raised(&exp))
    {
        //Console.WriteLine("Error: {0}", exp.psz_message);
        //throw new Win32Exception(exp.psz_message);
		throw system_error(static_cast<error_code::value_type>(-1), system_category(), exp.psz_message);
    }
    else
    {
        //_instanceVLC = (libvlc_instance_t)Marshal.PtrToStructure(pLibVLC, typeof(libvlc_instance_t));
        _pLibVLCHandle = pLibVLC;
        //Console.WriteLine("pLibVLC Done! - {0:X}", pLibVLC);
    }
}

void CLibVLCModule::LibVLCCoreInit()
{
	CStdString strLibVLCCoreFilePath = _strConfigPath + _cstrLibVLCCoreFileName;
	
	_hmodVLCCore = dlopen(strLibVLCCoreFilePath, RTLD_LAZY);
    if (NULL != _hmodVLCCore)
    {
		DWORD funcaddr = reinterpret_cast<DWORD>(dlsym(_hmodVLCCore, "input_item_SetMeta"));
        if (0 != funcaddr)
        {
            PVOID funcAccessNew = (PVOID)(funcaddr + 0x300);
            PVOID funcAccessDelete = (PVOID)(funcaddr + 0x240);
            _pLibAccess = new CLibVLCAccess(GetLibVLCObject(), funcAccessNew, funcAccessDelete);
        }
        else
        {
            //throw new Win32Exception(Marshal.GetLastWin32Error());
			throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
        }
    }
    else
    {
        //throw new Win32Exception(Marshal.GetLastWin32Error());
		throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), dlerror());
    }
}


CStdString CLibVLCModule::GetBestMacAddress()
{
    CStdString strBestMac;
    if (NULL != _hmodCALibVLCHook && NULL != _GetBestMacAddress)
    {
		BYTE arrMacAddr[6] = {0};
        //DWORD len = (DWORD)InvokeFuncPvoidPbyteDword(funcaddr, arrMacAddr, sizeof(arrMacAddr));
		DWORD len = _GetBestMacAddress(arrMacAddr, sizeof(arrMacAddr));
        if (len > 0)
        {
            //Object[] arrMacObj = new Object[6] { (byte)0, (byte)0, (byte)0, (byte)0, (byte)0, (byte)0 };
            //Array.Copy(arrMacAddr, arrMacObj, len);
            //strBestMac = string.Format("{0:x2}:{1:x2}:{2:x2}:{3:x2}:{4:x2}:{5:x2}", arrMacObj);
			strBestMac.Format("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", arrMacAddr[0], arrMacAddr[1], arrMacAddr[2], arrMacAddr[3], arrMacAddr[4], arrMacAddr[5]);
        }
    }
	else
	{
		//--------------------------------
		// Setup the hints address info structure
		// which is passed to the getaddrinfo() function
		struct addrinfo hints;
		ZeroMemory( &hints, sizeof(hints) );
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		//--------------------------------
		// Call getaddrinfo(). If the call succeeds,
		// the result variable will hold a linked list
		// of addrinfo structures containing response
		// information
		struct addrinfo *result = NULL;
		//XBMC->Log(LOG_DEBUG, "%s - _strCaHost: %s", __FUNCTION__, _strCaHost.c_str());
		DWORD dwRetval = getaddrinfo(_strCaHost, NULL, &hints, &result);
		if (0 == dwRetval)
		{
			struct addrinfo *ptr = NULL;
			// Retrieve each address and print out the hex bytes
			for(ptr = result; ptr != NULL; ptr = ptr->ai_next)
			{
				if(SOCK_STREAM == ptr->ai_socktype && IPPROTO_TCP == ptr->ai_protocol)
				{
					DWORD dwBestIfIndex;
					dwRetval = GetBestInterface(((struct sockaddr_in *)ptr->ai_addr)->sin_addr.s_addr, &dwBestIfIndex);

					if(NO_ERROR == dwRetval)
					{
						MIB_IFROW IfRow = {0};
						IfRow.dwIndex = dwBestIfIndex;
						if (NO_ERROR == (dwRetval = GetIfEntry(&IfRow)) && 6 == IfRow.dwPhysAddrLen)
						{
							strBestMac.Format("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
								IfRow.bPhysAddr[0], IfRow.bPhysAddr[1], IfRow.bPhysAddr[2],
								IfRow.bPhysAddr[3], IfRow.bPhysAddr[4], IfRow.bPhysAddr[5]);
						}
					}
				}
			}

			freeaddrinfo(result);
		}
		//else
		//{
			//printf("getaddrinfo failed with error: %d\n", dwRetval);
		//}
	}

    return strBestMac;
}

IStream* CLibVLCModule::NewAccess(const CStdString& strMrl)
{
	return _pLibAccess->OpenMRL(strMrl);
}


class CAUDPStream : public ComImplStream
{

friend class CLibVLCAccess;

	CAUDPStream(CLibVLCAccess::access_t* ptrAccess, CLibVLCAccess::FAccessDelete funcAccessDelete)
	{
        if (NULL != ptrAccess->pf_block)
        {
            _AccessVLCObject = ptrAccess;
            _funcAccessDelete = funcAccessDelete;
            _funcReadBlock = ptrAccess->pf_block;

            PINT pSysObject = reinterpret_cast<PINT>(ptrAccess) + CLibVLCAccess::access_t::SysOffset();
			if ('c' == ptrAccess->psz_access[0] && 'a' == ptrAccess->psz_access[1] && '\0' == ptrAccess->psz_access[2])
            {
                PINT pUdpObject = *reinterpret_cast<PINT*>(pSysObject);
                pUdpObject = *reinterpret_cast<PINT*>(pUdpObject);
                _udpSocket = *reinterpret_cast<SOCKET*>(pUdpObject + CLibVLCAccess::access_t::SysOffset());
            }
            else
            {
                _udpSocket = *reinterpret_cast<SOCKET*>(pSysObject);
            }

			//int nTimeout = 3000; // 3 seconds
			//if (SOCKET_ERROR == setsockopt(_udpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeout, sizeof(int)))
			//{
				//throw system_error(static_cast<error_code::value_type>(WSAGetLastError()), system_category(), "Error setting socket SO_RCVTIMEO opts.");
				//fprintf(stderr, "Error setting socket opts: %s\n", strerror(errno));
			//}

			fd_set fds;
			struct timeval tv={0};

			// Set up the file descriptor set.
			FD_ZERO(&fds) ;
			FD_SET(_udpSocket, &fds) ;

			// Set up the struct timeval for the timeout.
			tv.tv_sec = 14;

			// Wait until timeout or data received.
			int rc = select ( _udpSocket, &fds, NULL, NULL, &tv ) ;
			if (!rc)
			{
				SetLastError(WSAETIMEDOUT);
				throw system_error(static_cast<error_code::value_type>(GetLastError()), system_category(), "Initial read operation timed out.");
			}
			else if( SOCKET_ERROR == rc )
			{
				throw system_error(static_cast<error_code::value_type>(WSAGetLastError()), system_category(), "Initial read operation error.");
			}
        }
        else
        {
            //throw new System.ArgumentNullException("accessUDP.pf_block");
			throw invalid_argument("ptrAccess->pf_block");
        }

		_pLastBlock = NULL;
		_LastBlockOffset = 0;
	}

	virtual ~CAUDPStream()
	{
		if (NULL != _AccessVLCObject)
		{
			ReleaseLastBuffer();
			_funcAccessDelete(_AccessVLCObject);
			_AccessVLCObject = NULL;
			_funcAccessDelete = NULL;
			_funcReadBlock = NULL;
			_udpSocket = INVALID_SOCKET;
		}
	}

    // ISequentialStream Interface
public:
    virtual HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead)
    {
		HRESULT hr = S_OK;

		if (NULL != _AccessVLCObject)
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

protected:

	int ReadBuffer(PBYTE buffer, int offset, int count)
	{
		int iLen = ReadFromLastBlock(buffer, offset, count);

		offset += iLen;
		count -= iLen;

		if (count > 0)
		{
			PBLOCK pBlock = ReadBlock();
			if (NULL != pBlock)
			{
				int iBufferLen = min(count, pBlock->i_buffer);
				//Marshal.Copy(blockUDP.p_buffer, buffer, offset, iBufferLen);
				memcpy(buffer + offset, pBlock->p_buffer, iBufferLen);
				iLen += iBufferLen;
				count -= iBufferLen;

				if (count > 0)
				{
					ReleaseBlock(pBlock);
				}
				else
				{
					HoldLastBlock(pBlock, iBufferLen);
				}
			}
		}

		//XBMC->Log(LOG_DEBUG, "%s - iLen: %u", __FUNCTION__, iLen);

		return iLen;
	}

private:
	typedef CLibVLCAccess::block_t* PBLOCK;

	CLibVLCAccess::access_t* _AccessVLCObject;
	CLibVLCAccess::FAccessDelete _funcAccessDelete;
	CLibVLCAccess::block_t* (* _funcReadBlock)( CLibVLCAccess::access_t * );

	SOCKET _udpSocket;

	PBLOCK _pLastBlock;
	int _LastBlockOffset;

	void HoldLastBlock(PBLOCK pBlock, int offset)
	{
		if (NULL != pBlock)
		{
			if (offset < pBlock->i_buffer)
			{
				_pLastBlock = pBlock;
				_LastBlockOffset = offset;
			}
			else
			{
				ReleaseBlock(pBlock);
			}
		}
	}

	int ReadFromLastBlock(PBYTE buffer, int offset, int count)
	{
		int iLen = 0;
		if (NULL != _pLastBlock)
		{
			PBYTE pBuffer = _pLastBlock->p_buffer + _LastBlockOffset;
			iLen = min(count, (_pLastBlock->i_buffer - _LastBlockOffset));
			memcpy(buffer + offset, pBuffer, iLen);
			_LastBlockOffset += iLen;

			if (_LastBlockOffset == _pLastBlock->i_buffer)
			{
				ReleaseLastBuffer();
			}
		}

		return iLen;
	}

	void ReleaseLastBuffer()
	{
		if (NULL != _pLastBlock)
		{
			ReleaseBlock(_pLastBlock);
			_pLastBlock = NULL;
			_LastBlockOffset = 0;
		}
	}

	PBLOCK ReadBlock()
	{
		//IntPtr pBlock = InvokeImport.InvokeFunc(_funcReadBlock, _AccessVLCObject);
		//PBLOCK pBlock = reinterpret_cast<PBLOCK>(InvokeFuncPvoid(_funcReadBlock, _AccessVLCObject));
		PBLOCK pBlock = _funcReadBlock(_AccessVLCObject);

		return pBlock;
	}

	static void ReleaseBlock(PBLOCK pBlock)
	{
		//PVOID funcBlockRelease = pBlock->pf_release;
		//InvokeFuncPvoid(funcBlockRelease, reinterpret_cast<PVOID>(pBlock));
		pBlock->pf_release(pBlock);
	}
};

IStream* CLibVLCAccess::OpenMRL(string strMrl)
{
	XBMC->Log(LOG_DEBUG, "%s - %s", __FUNCTION__, strMrl.c_str());
    MRLParts parts(strMrl);
    //IntPtr pAccess = InvokeImport.InvokeFunc(_funcAccessNew, _LibVLCObject, parts.Access, parts.Demux, parts.Path);
	//access_t* pAccess = reinterpret_cast<access_t*>(InvokeFuncPvoidStrStrStr(_funcAccessNew, _LibVLCObject, parts.Access(), parts.Demux(), parts.Path()));
	access_t* pAccess = _funcAccessNew(_LibVLCObject, parts.Access(), parts.Demux(), parts.Path());

    IStream* stream = NULL;

    if (NULL != pAccess)
    {
		try
		{
			stream = new CAUDPStream(pAccess, _funcAccessDelete);
		}
		catch(const exception& excp)
		{
			XBMC->Log(LOG_ERROR, "Open udp stream (%s) failed, error: %s", strMrl.c_str(), excp.what());
			_funcAccessDelete(pAccess);
		}
    }
    else
    {
        //throw new System.ArgumentNullException("pAccess");
		//throw system_error(static_cast<error_code::value_type>(-1), system_category(), "pAccess");
		XBMC->Log(LOG_ERROR, "Object of stream access is null.");
    }

    return stream;
}


#if 0
class CAUDPStreamBuf : public std::streambuf
{
public:
	CAUDPStreamBuf()
	{
	}
protected:
	virtual int underflow() 
	{
		/*setg(buf, buf, buf+1); 
		if (recv(*s, buf, 1, 0) != 1)
			return EOF;
		return buf[0];*/

		return 0;
	}

private:
};

class CAUDPSTDStream: public std::istream
{
protected:
	CAUDPStreamBuf _strBuf;
	bool open;
public:
	CAUDPSTDStream(std::string address, int port): std::istream( &_strBuf )
	{
	}
	bool isOpen()
	{
		return open;
	}
	void closeConnection()
	{
		//closesocket(s);
	}
};

int main()
{
	using namespace std;
 
	WSADATA data;
	WSAStartup(2,&amp;data);
	//Create a custom ostream object with our own
	//stream buffer
	socketstream sock(&quot;213.67.169.210&quot;, 80);
	if (!sock.isOpen())
		return 1;
	sock &lt;&lt; &quot;GET / HTTP/1.1\n\n&quot;;
	std::string s;
	while (!sock.eof())
	{
		getline( sock, s );
		cout &lt;&lt; s;
	}
	cout &lt;&lt; &quot;\n\n\nConnection closed.&quot;;
	cin.get();
}
#endif // 0

} //namespace LibVLCCAPlugin