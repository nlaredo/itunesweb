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
L"input{font-family:monospace;}\r\n"
L"table{border:0;margin-left:auto;margin-right:auto;margin-bottom:10px;}\r\n"
L"th{background:#bbd;padding:2px;}\r\n"
L"td{padding:2px;border-bottom:1px dotted #bbd;}\r\n"
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
L"	var h = parseInt(tmp[0],10);\r\n"
L"	var m = parseInt(tmp[1],10);\r\n"
L"	var s = parseInt(tmp[2],10);\r\n"
L"	tmp = total.split(':');\r\n"
L"	var th = parseInt(tmp[0],10);\r\n"
L"	var tm = parseInt(tmp[1],10);\r\n"
L"	var ts = parseInt(tmp[2],10);\r\n"
L"	if (++s > 59) { s = 0; if (++m > 59) { m = 0; h++; } }\r\n"
L"	document.getElementById(\"now\").innerHTML =\r\n"
L"		h + ':' + lz(m) + ':' + lz(s);\r\n"
L"	if (state == 'Playing') {\r\n"
L"		if (h >= th && m >= tm && s >= ts) window.location=\"/\";\r\n"
L"		else setTimeout('newtime();', 1000);\r\n"
L"	}\r\n"
L"}\r\n"
L"</script>\r\n"
L"</head>\r\n"
L"<body onload=\"newtime();\">\r\n"
L"<form action=\"/\" method=\"post\">\r\n<table width=\"100%%\">\r\n"
L"<tr><td>Track</td><td id=\"track\" colspan=7>%ls</td><td>Vol</td></tr>\r\n"
L"<tr><td>Album</td><td id=\"album\" colspan=7>%ls</td><td>\r\n"
L"<input type=\"submit\" name=\"10up\" value=\"+10\" /></td></tr>\r\n"
L"<tr><td>Artist</td><td id=\"artist\" colspan=7>%ls</td><td>\r\n"
L"<input type=\"submit\" name=\"up\" value=\"+\" /></td></tr>\r\n"
L"<tr><td>Comment</td><td id=\"comment\" colspan=7>%ls</td><td>\r\n"
L"<input type=\"submit\" name=\"mute\" value=\"0\" /> %ld&nbsp;%%</td>\r\n"
L"</tr><tr><td id=\"state\" colspan=8>%ls</td><td>\r\n"
L"<input type=\"submit\" name=\"down\" value=\"-\" /></td></tr>\r\n"
L"<tr><td id=\"now\">%ld:%02ld:%02ld</td>\r\n"
L"<td><input type=\"submit\" name=\"prev\" value=\"|<<\" /></td>\r\n"
L"<td><input type=\"submit\" name=\"rew\" value=\"<<\" /></td>\r\n"
L"<td><input type=\"submit\" name=\"pause\" value=\"||\" /></td>\r\n"
L"<td><input type=\"submit\" name=\"play\" value=\">\" /></td>\r\n"
L"<td><input type=\"submit\" name=\"ffwd\" value=\">>\" /></td>\r\n"
L"<td><input type=\"submit\" name=\"next\" value=\">>|\" /></td>\r\n"
L"<td id=\"total\">%ld:%02ld:%02ld</td><td>\r\n"
L"<input type=\"submit\" name=\"10down\" value=\"-10\" /></td></tr>\r\n"
L"</table>\r\n"
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
  wstring comment = L"", state, version = L"";
  BSTR bstr = 0;
  long position, duration, volume;
  string strRet;
  char *postdata = strstr(req, "\r\n\r\n");

  if (!postdata) {
    postdata = req + reqlen;
  }
  // FIXME: do real http header processing to handle multiple files
  // handle any requests before updating state
  if (strstr(postdata, "prev"))
    iITunes->BackTrack();
  if (strstr(postdata, "next"))
    iITunes->NextTrack();
  if (strstr(postdata, "ffwd")) {
    iITunes->get_PlayerState(&iIPlayerState);
    if (iIPlayerState == ITPlayerStatePlaying)
      iITunes->FastForward();
    else
      iITunes->Resume();
  }
  if (strstr(postdata, "rew")) {
    iITunes->get_PlayerState(&iIPlayerState);
    if (iIPlayerState == ITPlayerStatePlaying)
      iITunes->Rewind();
    else
      iITunes->Resume();
  }
  if (strstr(postdata, "play")) {
    iITunes->get_PlayerState(&iIPlayerState);
    if (iIPlayerState == ITPlayerStateStopped)
      iITunes->Play();
    else
      iITunes->Resume();
  }
  if (strstr(postdata, "pause")) {
    iITunes->get_PlayerState(&iIPlayerState);
    if (iIPlayerState == ITPlayerStateStopped)
      iITunes->Play();
    else
      iITunes->Pause();
  }
  if (strstr(postdata, "10up")) {
    iITunes->get_SoundVolume(&volume);
    volume += 10;
    iITunes->put_SoundVolume(volume);
  } else if (strstr(postdata, "up")) {
    iITunes->get_SoundVolume(&volume);
    volume += 2;
    iITunes->put_SoundVolume(volume);
  }
  if (strstr(postdata, "10down")) {
    iITunes->get_SoundVolume(&volume);
    volume -= 10;
    if (volume < 0)
      volume = 0;
    iITunes->put_SoundVolume(volume);
  } else if (strstr(postdata, "down")) {
    iITunes->get_SoundVolume(&volume);
    volume -= 2;
    if (volume < 0)
      volume = 0;
    iITunes->put_SoundVolume(volume);
  }
  if (strstr(postdata, "mute")) {
    static long oldvol = 13;
    iITunes->get_SoundVolume(&volume);
    if (volume)
      iITunes->put_SoundVolume(0);
    else
      iITunes->put_SoundVolume(oldvol);
    oldvol = volume;
  }

  // update state, may be affected by above requests...
  iITunes->get_CurrentTrack(&iITrack);
  iITunes->get_CurrentTrack(&iITrack);
  iITunes->get_PlayerState(&iIPlayerState);
  iITunes->get_PlayerPosition(&position);
  iITunes->get_SoundVolume(&volume);
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
    iITrack->get_Duration(&duration);
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
		  album.c_str(), artist.c_str(), comment.c_str(), volume,
		  state.c_str(), position / 3600, (position / 60) % 60,
		  position % 60, duration / 3600, (duration / 60) % 60,
		  duration % 60);
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
