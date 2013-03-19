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
#pragma once

#include "platform/util/StdString.h"
#include "client.h"

#define DllImport   __declspec( dllimport )

typedef std::vector<CStdString> CStdStringArray;

namespace LibVLCCAPlugin 
{

class ILibVLCModule
{
public:
	static ILibVLCModule* NewModule(const CStdString& cstrConfigPath, const CStdString& cstrCaUri, ULONG ulMCastIPAddr);
	static void DeleteModule(ILibVLCModule* pmodule) { if(pmodule) delete pmodule; }

	virtual ~ILibVLCModule() {}
	virtual IStream* NewAccess(const CStdString& strMrl) = 0;
	virtual CStdString GetBestMacAddress() = 0;
};

} //namespace LibVLCCAPlugin