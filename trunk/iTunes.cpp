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
#include <time.h>

//////////////////////////////////////////////////////////////////////////////

#include "iTunesCOMInterface.h"

using namespace std;

static IiTunes *iITunes = 0;

//////////////////////////////////////////////////////////////////////////////
const static wchar_t *htmlfmt =
L"<html><head><title>iTunesWeb Remote - iTunes %ls</title>\r\n"
L"<style type=\"text/css\">\r\nbody{font-family:Sans-Serif}\r\n"
L"th{padding:2px;background:#bbd}\r\n"
L"td{padding:2px;border-bottom:1px dotted #bbd;}\r\n"
L"</style>\r\n"
L"<script type=\"text/javascript\">\r\n"
L"function lz(n) { return (n < 10) ? '0' + n : n; }\r\n"
L"function newtime()\r\n"
L"{\r\n"
L"	var now = document.getElementById('now').innerHTML;\r\n"
L"	var total = document.getElementById('total').innerHTML;\r\n"
L"	var state = document.getElementById('state').innerHTML;\r\n"
L"	var tmp = new Array();\r\n"
L"	tmp = now.split(':');\r\n"
L"	var h = parseInt(tmp[0],10);\r\n"
L"	var m = parseInt(tmp[1],10);\r\n"
L"	var s = parseInt(tmp[2],10);\r\n"
L"	tmp = total.split(':');\r\n"
L"	var th = parseInt(tmp[0],10);\r\n"
L"	var tm = parseInt(tmp[1],10);\r\n"
L"	var ts = parseInt(tmp[2],10);\r\n"
L"	if (tmp[0].substring(0,1) == '-') { th = -th; }\r\n"
L"	else { th -= h; tm -= m; ts -= s; }\r\n"
L"	ts--;\r\n"
L"	if (ts < 0) { ts += 60; tm-- };\r\n"
L"	if (tm < 0) { tm += 60; th-- };\r\n"
L"	if (++s > 59) { s = 0; if (++m > 59) { m = 0; h++; } }\r\n"
L"	document.getElementById('now').innerHTML =\r\n"
L"		h + ':' + lz(m) + ':' + lz(s);\r\n"
L"	document.getElementById('total').innerHTML =\r\n"
L"		'-' + th + ':' + lz(tm) + ':' + lz(ts);\r\n"
L"	if (state == 'play') {\r\n"
L"		if (th <= 0 && tm <= 0 && ts <= 0) window.location='/';\r\n"
L"		else setTimeout('newtime();', 1000);\r\n"
L"	}\r\n"
L"	document.getElementsByName(state)[0].style.color='#bbd';\r\n"
L"}\r\n"
L"</script>\r\n"
L"</head>\r\n"
L"<body onload=\"newtime();\">\r\n"
L"<form action=\"/\" method=\"post\">\r\n<table width=\"100%%\" rules="
L"groups>\r\n<tr><th id=\"now\">%ld:%02ld:%02ld</th>\r\n"
L"<th><input type=\"submit\" name=\"prev\" value=\"|<<\" /></th>\r\n"
L"<th><input type=\"submit\" name=\"rew\" value=\"<<\" /></th>\r\n"
L"<th><input type=\"submit\" name=\"pause\" value=\"||\" /></th>\r\n"
L"<th><input type=\"submit\" name=\"play\" value=\">\" /></th>\r\n"
L"<th><input type=\"submit\" name=\"ffwd\" value=\">>\" /></th>\r\n"
L"<th><input type=\"submit\" name=\"next\" value=\">>|\" /></th>\r\n"
L"<th id=\"total\">%ld:%02ld:%02ld</td><th>Volume</th>"
L"</tr>\r\n<tr><td>Track</td><td id=\"track\" colspan=7>%ls</td><td>\r\n"
L"<input type=\"submit\" name=\"10up\" value=\"+10\" /></td></tr>\r\n"
L"<tr><td>Album</td><td id=\"album\" colspan=7>%ls</td><td>\r\n"
L"<input type=\"submit\" name=\"up\" value=\"+\" /></td></tr>\r\n"
L"<tr><td>Artist</td><td id=\"artist\" colspan=7>%ls</td><td>\r\n"
L"<input type=\"submit\" name=\"mute\" value=\"0\" /> %ld&nbsp;%%</td>\r\n"
L"<tr><td>Comment</td><td id=\"comment\" colspan=7>%ls</td><td>\r\n"
L"<input type=\"submit\" name=\"down\" value=\"-\" /></td></tr>\r\n"
L"</tr><td>State</td><th id=\"state\">%ls</th><td colspan=4>\r\n"
L"<input type=\"radio\" onchange=\"this.form.submit();\" name=\"ord\" value="
L"\"shuf\" %ls>Shuffle</td>\r\n<td colspan=2><input type=\"radio\" "
L"name=\"ord\" value=\"seq\" onchange=\"this.form.submit();\" "
L"%ls>Sequential</td>\r\n<td><input type=\"submit\" name=\"10down\" "
L"value=\"-10\" /></td></tr>\r\n</table></form>\r\n<p>"
L"<a href=\"/\">Refresh</a></p>\r\n<table width=\"100%%\" rules=groups>\r\n"
L"<tr><th>#</th><th>Name</th><th>Artist</th><th>Time</th>\r\n"
L"<th>Album</th><th>Genre</th>\r\n"
L"<th>Play Count</th><th>Last Played</th><th>Comment</th></tr>\r\n";

const static wchar_t *listfmt =
L"<tr><td>%d</td><td>%ls</td><td>%ls</td>\r\n"
L"<td>%ld:%02ld:%02ld</td><td>%ls</td><td>%ls</td>\r\n"
L"<td>%ld</td><td>%ls</td><td>%ls</td></tr>\r\n";

const static wchar_t *htmltail = L"</table></body></html>\r\n";

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
  IITTrack *iITrack = NULL;
  ITPlayerState iIPlayerState;
  IITPlaylist *iPlaylist = NULL;
  IITTrackCollection *iTracks = NULL;
  VARIANT_BOOL do_shuffle;
  wstring version = L"", state;
  BSTR bstr = 0;
  long position, duration, volume, play_index = 0;
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
  iITunes->get_CurrentPlaylist(&iPlaylist);
  if (strstr(postdata, "shuf") && iITunes) {
    if (iPlaylist) {
      iPlaylist->get_Shuffle(&do_shuffle);
      if (!do_shuffle)
	iPlaylist->put_Shuffle(VARIANT_TRUE);
    }
  }
  if (strstr(postdata, "seq") && iITunes) {
    if (iPlaylist) {
      iPlaylist->get_Shuffle(&do_shuffle);
      if (do_shuffle)
	iPlaylist->put_Shuffle(VARIANT_FALSE);
    }
  }

  // update state, may be affected by above requests...
  iITunes->get_CurrentTrack(&iITrack);
  if (iITrack)
    iITrack->get_PlayOrderIndex(&play_index);
  iITunes->get_PlayerState(&iIPlayerState);
  iITunes->get_PlayerPosition(&position);
  iITunes->get_SoundVolume(&volume);
  iITunes->get_Version((BSTR *)&bstr);
  if (bstr)
    version = bstr;

  if (iPlaylist) {
    iPlaylist->get_Shuffle(&do_shuffle);
    iPlaylist->get_Tracks(&iTracks);
  }

  if (iITrack)
    iITrack->Release();

  switch (iIPlayerState) {
  case ITPlayerStatePlaying:
    state = L"play";
    break;
  case ITPlayerStateStopped:
    state = L"pause";
    break;
  case ITPlayerStateFastForward:
    state = L"ffwd";
    break;
  case ITPlayerStateRewind:
    state = L"rew";
    break;
  default:
    state = L"unkn";
    break;
  }
  for (long index = 0; iTracks && index < 7; index++) {
    wstring track = L"", album = L"", url = L"", artist = L"", comment = L"";
    wstring genre = L"";
    long playcount = 0;
    DATE lastplayed = 0;
    iITrack = NULL;
    iTracks->get_ItemByPlayOrder(play_index + index, &iITrack);
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
      iITrack->get_Genre((BSTR *)&bstr);
      if (bstr)
	genre = bstr;
      iITrack->get_Duration(&duration);
      iITrack->get_PlayedCount(&playcount);
      iITrack->get_PlayedDate(&lastplayed);
      iITrack->Release();
    }
    if (index == 0) {
      // Convert the result from wchar_t to utf-8 string
      StringCbPrintfW(htmlbuf, 8191, htmlfmt, version.c_str(), position / 3600,
		      (position / 60) % 60, position % 60, duration / 3600,
		      (duration / 60) % 60, duration % 60, track.c_str(),
		      album.c_str(), artist.c_str(), volume, comment.c_str(),
		      state.c_str(), do_shuffle ? L"checked" : L"", do_shuffle ?
		      L"": L"checked");
    }
    size_t len = wcslen(htmlbuf);
    __time64_t when = (__time64_t)((double)lastplayed > 25569 ?
		      (((double)lastplayed - 25569) * 86400) : 0);
    struct tm *tmp = _gmtime64(&when);
    StringCbPrintfW(htmlbuf + len, 8191 - len, listfmt, index + play_index,
		    track.c_str(), artist.c_str(),
		    duration / 3600, (duration / 60) % 60, duration % 60,
		    album.c_str(), genre.c_str(), playcount,
		    when ? _wasctime(tmp) : L"", comment.c_str());
  }
  size_t len = wcslen(htmlbuf);
  StringCbPrintfW(htmlbuf + len, 8192 - len, htmltail);
  len = wcslen(htmlbuf);
  // max 8192 bytes in return message...
  char convbuf[8192];
  memset(convbuf, 0, 8192);
  WideCharToMultiByte(CP_UTF8, 0, htmlbuf, len,
		      convbuf, 8191, NULL, NULL);
  strRet = convbuf;
  return strRet;
}

//////////////////////////////////////////////////////////////////////////////
