//////////////////////////////////////////////////////////////////////////////
// iTunes.cpp - Use COM interface to iTunes to build webpages/control iTunes
//  Copyright (C) 2008  Nathan Laredo <laredo@gnu.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//////////////////////////////////////////////////////////////////////////////

#include <string>
#include <windows.h>

//////////////////////////////////////////////////////////////////////////////

#include "iTunesCOMInterface.h"

static IiTunes *iITunes = 0;

//////////////////////////////////////////////////////////////////////////////

void kill_iTunes(void)
{
	if(iITunes)
		iITunes->Release();
	iITunes = 0;
	CoUninitialize();
}

//////////////////////////////////////////////////////////////////////////////

void init_iTunes(void)
{
	HRESULT hRes;

	if (iITunes)
		return;		// already initialized

	hRes = CoInitialize(0);
	if (hRes != S_OK) {
		exit(1);
	}
	hRes = ::CoCreateInstance(CLSID_iTunesApp, NULL, CLSCTX_LOCAL_SERVER, IID_IiTunes, (PVOID *)&iITunes);
	if (hRes == S_OK && iITunes) {
		// iTunes interface created successfully...
	} else {
		kill_iTunes();
		exit(1);
	}
}

//////////////////////////////////////////////////////////////////////////////

std::string get_iTunes(void)
{
	using std::string;
	using std::wstring;

	IITTrack *iITrack = 0;
	ITPlayerState iIPlayerState;
	//IITPlaylist iIPlaylist;
	//IITTrackCollection iITrackCollection;
	BSTR bstr = 0;
	long position;
	wchar_t posbuf[64];

	// String operations done in a wstring, then converted for return
	wstring wstrRet;
	string strRet;

	iITunes->get_CurrentTrack(&iITrack);
	iITunes->get_PlayerState(&iIPlayerState);
#if 0
	iITunes->get_CurrentPlaylist(&iIPlaylist);

	if (iIPlaylist) {
		iIPlaylist->get_Time((BSTR *)&bstr);
		if (bstr) {
			wstrRet += L"Playlist Time: ";
			wstrRet += bstr;
			wstrRet += L"\r\n";
		}
	}
	bstr = 0;
#endif
	iITunes->get_CurrentStreamURL((BSTR *)&bstr);
	if (bstr) {
		wstrRet += L"URL: ";
		wstrRet += bstr;
		wstrRet += L"\r\n";
	}

	iITunes->get_PlayerPosition(&position);
	wsprintfW(posbuf, L"%02d:%02d", position / 60, position % 60);
	wstrRet += L"Position: ";
	wstrRet += posbuf;
	wstrRet += L"\r\n";

	if(iITrack) {
		bstr = 0;
		iITrack->get_Name((BSTR *)&bstr);
		// Add song title
		if(bstr) {
			wstrRet += L"Track: ";
			wstrRet += bstr;
			wstrRet += L"\r\n";
		}
		bstr = 0;
		iITrack->get_Album((BSTR *)&bstr);
		if (bstr) {
			wstrRet += L"Album: ";
			wstrRet += bstr;
			wstrRet += L"\r\n";
		}
		bstr = 0;
		iITrack->get_Artist((BSTR *)&bstr);
		if (bstr) {
			wstrRet += L"Artist: ";
			wstrRet += bstr;
			wstrRet += L"\r\n";
		}
		bstr = 0;
		iITrack->get_Comment((BSTR *)&bstr);
		if (bstr) {
			wstrRet += L"Comment: ";
			wstrRet += bstr;
			wstrRet += L"\r\n";
		}
		bstr = 0;
		iITrack->get_Time((BSTR *)&bstr);
		if (bstr) {
			wstrRet += L"Time: ";
			wstrRet += bstr;
			wstrRet += L"\r\n";
		}
	} else {
		// Couldn't get track name
	}


	wstrRet += L"State: ";
	// Add player state
	switch(iIPlayerState)
	{
		case ITPlayerStatePlaying:
			wstrRet += L"Playing";
			break;
		case ITPlayerStateStopped:
			wstrRet += L"Stopped";
			break;
		case ITPlayerStateFastForward:
			wstrRet += L"FastForward";
			break;
		case ITPlayerStateRewind:
			wstrRet += L"Rewind";
			break;
		default:
			break;
	}
	wstrRet += L"\r\n";

	if(iITrack)
		iITrack->Release();

	// Convert the result from wstring to utf-8 string
	size_t len = wstrRet.length();
	// max 1024 bytes in return message...
	char convbuf[1024];
	memset(convbuf, 0, 1024);
	WideCharToMultiByte(CP_UTF8, 0, wstrRet.c_str(), len,
		convbuf, 1024, NULL, NULL);
	strRet = convbuf;
	return strRet;
}

//////////////////////////////////////////////////////////////////////////////
