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
#include <strsafe.h>

//////////////////////////////////////////////////////////////////////////////

#include "iTunesCOMInterface.h"

using namespace std;

static IiTunes *iITunes = 0;

//////////////////////////////////////////////////////////////////////////////
const static wchar_t *htmlfmt =
L"<html><head><title>iTunesWeb Remote - iTunes %ls</title>\r\n"
L"<style type=\"text/css\">\r\n"
L"body{font-family:monospace;}\r\n"
L"input{font-family:monospace;}\r\n"
L"</style>\r\n"
L"<script type=\"text/javascript\">\r\n"
L"function lz(n) { return (n < 10) ? '0' + n : n; }\r\n"
L"function newtime()\r\n"
L"{\r\n"
L"	var now = document.getElementById(\"now\").innerHTML;\r\n"
L"	var total = document.getElementById(\"total\").innerHTML;\r\n"
L"	var state = document.getElementById(\"state\").innerHTML;\r\n"
L"	var tmp = new Array();\r\n"
L"	tmp = now.split(':');\r\n"
L"	var m = parseInt(tmp[0],10);\r\n"
L"	var s = parseInt(tmp[1],10);\r\n"
L"	tmp = total.split(':');\r\n"
L"	var tm = parseInt(tmp[0],10);\r\n"
L"	var ts = parseInt(tmp[1],10);\r\n"
L"	if (++s > 59) { s = 0; m++; }\r\n"
L"	document.getElementById(\"now\").innerHTML = lz(m) + ':' + lz(s);\r\n"
L"	if (m >= tm && s > ts) window.location=\"i.html\";\r\n"
L"	else if (state == 'Playing') setTimeout('newtime();', 1000);\r\n"
L"}\r\n"
L"</script>\r\n"
L"</head>\r\n"
L"<body onload=\"newtime();\">\r\n"
L"<form action=\"i.html\" method=\"get\">\r\n"
L"Track: <span id=\"track\">%ls</span><br />\r\n"
L"Album: <span id=\"album\">%ls</span><br />\r\n"
L"Artist: <span id=\"artist\">%ls</span><br />\r\n"
L"Comment: <span id=\"comment\">%ls</span><br />\r\n"
L"<span id=\"state\">%s</span><br />\r\n"
L"<p>\r\n"
L"<span id=\"now\">%02ld:%02ld</span>\r\n"
L"<input type=\"submit\" name=\"prev\" value=\"|<<\" />\r\n"
L"<input type=\"submit\" name=\"rew\" value=\"<<\" />\r\n"
L"<input type=\"submit\" name=\"pause\" value=\"||\" />\r\n"
L"<input type=\"submit\" name=\"play\" value=\">\" />\r\n"
L"<input type=\"submit\" name=\"ffwd\" value=\">>\" />\r\n"
L"<input type=\"submit\" name=\"next\" value=\">>|\" />\r\n"
L"<span id=\"total\">%ls</span>\r\n"
L"</p>\r\n"
L"<p>Volume<br />\r\n"
L"<input type=\"submit\" name=\"10up\" value=\"+10\" /><br />\r\n"
L"<input type=\"submit\" name=\"up\" value=\"+\" /><br />\r\n"
L"<input type=\"submit\" name=\"mute\" value=\"0\" /> %ld %% (%ls)<br />\r\n"
L"<input type=\"submit\" name=\"down\" value=\"-\" /><br />\r\n"
L"<input type=\"submit\" name=\"10down\" value=\"-10\" /><br />\r\n"
L"</p>\r\n"
L"</form></body></html>\r\n";

static wchar_t htmlbuf[8192];	// storage for above after wsprintfW...

//////////////////////////////////////////////////////////////////////////////

void kill_iTunes(void)
{
  if (iITunes)
    iITunes->Release();
  iITunes = 0;
  CoUninitialize();
}

//////////////////////////////////////////////////////////////////////////////

void init_iTunes(void)
{
  HRESULT hRes;

  if (iITunes)
    return;			// already initialized

  hRes = CoInitialize(0);
  if (hRes != S_OK) {
    exit(1);
  }
  hRes =::CoCreateInstance(CLSID_iTunesApp, NULL, CLSCTX_LOCAL_SERVER,
			   IID_IiTunes, (PVOID *) & iITunes);
  if (hRes == S_OK && iITunes) {
    // iTunes interface created successfully...
  } else {
    kill_iTunes();
    exit(1);
  }
}

//////////////////////////////////////////////////////////////////////////////

std::string get_iTunes(char *req, int reqlen)
{
  IITTrack *iITrack = 0;
  ITPlayerState iIPlayerState;
  wstring track = L"", album = L"", url = L"", artist = L"";
  wstring comment = L"", total = L"00:00", state, version = L"";
  BSTR bstr = 0;
  long position, volume;
  VARIANT_BOOL isMuted;
  string strRet;

  req[reqlen] = 0;	// prevent walking into neverneverland
  // handle any requests before updating state
  if (strstr(req, "prev"))
    iITunes->BackTrack();
  if (strstr(req, "next"))
    iITunes->NextTrack();
  if (strstr(req, "ffwd")) {
    iITunes->get_PlayerState(&iIPlayerState);
    if (iIPlayerState == ITPlayerStatePlaying)
      iITunes->FastForward();
    else
      iITunes->Resume();
  }
  if (strstr(req, "rew")) {
    iITunes->get_PlayerState(&iIPlayerState);
    if (iIPlayerState == ITPlayerStatePlaying)
      iITunes->Rewind();
    else
      iITunes->Resume();
  }
  if (strstr(req, "play"))
    iITunes->Play();
  if (strstr(req, "pause"))
    iITunes->Pause();
  if (strstr(req, "10up")) {
    iITunes->get_SoundVolume(&volume);
    iITunes->put_SoundVolume(volume + 10);
  } else if (strstr(req, "up")) {
    iITunes->get_SoundVolume(&volume);
    iITunes->put_SoundVolume(volume + 1);
  }
  if (strstr(req, "10down")) {
    iITunes->get_SoundVolume(&volume);
    if (volume < 10)
      volume = 10;
    iITunes->put_SoundVolume(volume - 10);
  } else if (strstr(req, "down")) {
    iITunes->get_SoundVolume(&volume);
    if (volume < 1)
      volume = 1;
    iITunes->put_SoundVolume(volume - 1);
  }
  if (strstr(req, "mute")) {
    iITunes->get_Mute(&isMuted);
    iITunes->put_Mute(!isMuted);
  }

  // update state, may be affected by above requests...
  iITunes->get_CurrentTrack(&iITrack);
  iITunes->get_CurrentTrack(&iITrack);
  iITunes->get_PlayerState(&iIPlayerState);
  iITunes->get_PlayerPosition(&position);
  iITunes->get_SoundVolume(&volume);
  iITunes->get_Mute(&isMuted);
  iITunes->get_Version((BSTR *)&bstr);
  if (bstr)
    version = bstr;

  if (iITrack) {
    bstr = 0;
    iITrack->get_Name((BSTR *)&bstr);
    if (bstr)
      track = bstr;
    bstr = 0;
    iITrack->get_Album((BSTR *)&bstr);
    if (bstr)
      album = bstr;
    bstr = 0;
    iITrack->get_Artist((BSTR *)&bstr);
    if (bstr)
      artist = bstr;
    bstr = 0;
    iITrack->get_Comment((BSTR *)&bstr);
    if (bstr)
      comment = bstr;
    bstr = 0;
    iITrack->get_Time((BSTR *)&bstr);
    if (bstr)
      total = bstr;
  }

  switch (iIPlayerState) {
  case ITPlayerStatePlaying:
    state = L"Playing";
    break;
  case ITPlayerStateStopped:
    state = L"Stopped";
    break;
  case ITPlayerStateFastForward:
    state = L"FastForward";
    break;
  case ITPlayerStateRewind:
    state = L"Rewind";
    break;
  default:
    state = L"Unknown";
    break;
  }
  if (iITrack)
    iITrack->Release();

  // Convert the result from wchar_t to utf-8 string
  StringCbPrintfW(htmlbuf, 8191, htmlfmt, version.c_str(), track.c_str(),
		  album.c_str(), artist.c_str(), comment.c_str(),
		  state.c_str(), position / 60, position % 60,
		  total.c_str(), volume, isMuted ? L"Muted" :
		  L"Unmuted");
  size_t len = wcslen(htmlbuf);
  // max 8192 bytes in return message...
  char convbuf[8192];
  memset(convbuf, 0, 8192);
  WideCharToMultiByte(CP_UTF8, 0, htmlbuf, len,
		      convbuf, 8191, NULL, NULL);
  strRet = convbuf;
  return strRet;
}

//////////////////////////////////////////////////////////////////////////////
