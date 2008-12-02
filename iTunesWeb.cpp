//////////////////////////////////////////////////////////////////////////////
// iTunesWeb.cpp - Provide a HTTP interface to iTunes for status and control
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

#define _WIN32_IE 0x0600

#include <iostream>
#include <string>
#include <winsock2.h>
#include <time.h>
#include "resource.h"

// TODO: split gui bits out into another file...
#include <commctrl.h>
#include <windowsx.h>
#include <shellapi.h>
static TCHAR AppClassName[MAX_RESOURCESTRING + 1];
static TCHAR AppTitle[MAX_RESOURCESTRING + 1];
static HWND hwndMain = NULL;
static NOTIFYICONDATA nid;

using namespace std;

// listen for incoming connections on this port
#define HTTP_PORT	8080

//////////////////////////////////////////////////////////////////////////////

extern void init_iTunes(void);
extern void kill_iTunes(void);
extern string get_iTunes(char *req, int reqlen);

//////////////////////////////////////////////////////////////////////////////
// maximum interesting length of incoming network packet
#define MAXNETB 2048
// maximum length of an individual incoming request
#define MAXREQB 8192
// maximum number of connections to allow
#define MAXCONN (FD_SETSIZE - 5)

static char netbuf[MAXNETB];
static struct in_addr fromhost[MAXCONN];
static SOCKET sockfd[MAXCONN];
static char *reqbuf[MAXCONN];
static int reqlen[MAXCONN];
static int netlen = 0, sockfdmax = 0;
static fd_set readfs;

//////////////////////////////////////////////////////////////////////////////
// this removes a connect from the list of connected sockets, hosts
// s = index of socket to remove
void RemoveConnect(int s)
{
  int i;

  cout << "Close " << inet_ntoa(fromhost[s]) << endl;
  shutdown(sockfd[s], 2);
  closesocket(sockfd[s]);
  i = (--sockfdmax) - s;
  // free request assembly buffer
  if (reqbuf[s]) {
    free(reqbuf[s]);
    reqbuf[s] = NULL;
  }
  reqlen[s] = 0;
  if (!i)
    return;			// no need to collapse list if at end
  memmove(&sockfd[s], &sockfd[s + 1], i * sizeof(*sockfd));
  memmove(&fromhost[s], &fromhost[s + 1], i * sizeof(*fromhost));
  memmove(&reqbuf[s], &reqbuf[s + 1], i * sizeof(*reqbuf));
  memmove(&reqlen[s], &reqlen[s + 1], i * sizeof(*reqlen));
}

//////////////////////////////////////////////////////////////////////////////
// accept a new connection, indicated by positive select on listen socket
SOCKET AcceptConnect(void)
{
  struct sockaddr_in sa;
  int u = sizeof(sa);
  SOCKET s;

  memset(&sa, 0, u);
  s = accept(sockfd[0], (struct sockaddr *) &sa, &u);
  memcpy(&fromhost[sockfdmax], (struct in_addr *) &sa.sin_addr,
	 sizeof(*fromhost));
  //fcntl(s, F_SETFL, O_NDELAY);
  cout << "Open " << inet_ntoa(fromhost[sockfdmax]) << endl;
  return s;

}

//////////////////////////////////////////////////////////////////////////////
// read as much as is available, zero length read or error = return 0
int ReadInput(int s)
{
  netlen = recvfrom(sockfd[s], netbuf, MAXNETB, 0, NULL, NULL);
  if (netlen <= 0) {
    netlen = 0;
  }
  return netlen;
}

//////////////////////////////////////////////////////////////////////////////

void init_server(void)
{
  WSADATA wsaData;
  sockaddr_in sa;
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

  sockfd[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd[0] == INVALID_SOCKET) {
    WSACleanup();
    exit(1);
  }
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = PF_INET;
  sa.sin_port = htons(HTTP_PORT);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sockfd[0], (SOCKADDR *) & sa, sizeof(sa)) == SOCKET_ERROR) {
    closesocket(sockfd[0]);
    WSACleanup();
    exit(1);
  }
  listen(sockfd[0], SOMAXCONN);
  sockfdmax++;
}

void cleanup_exit(int status)
{
  kill_iTunes();
  shutdown(sockfd[0], 2);
  closesocket(sockfd[0]);
  WSACleanup();
  exit(status);
}

//////////////////////////////////////////////////////////////////////////////
static void DlgOnCommand(HWND hwnd, int id, HWND hwndCtl, UINT cNotify)
{
  switch (id) {
    case IDOK:
    case IDCANCEL:
      EndDialog(hwnd, TRUE);
  }
}

//////////////////////////////////////////////////////////////////////////////
INT_PTR CALLBACK DlgProc(HWND hwnd, UINT message,
			 WPARAM wParam, LPARAM lParam)
{
  switch(message) {
    case WM_INITDIALOG:
      return TRUE;
    case WM_COMMAND:
      return HANDLE_WM_COMMAND(hwnd, wParam, lParam, DlgOnCommand);
  }
  return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
static void del_notify_icon(void)
{
  ShowWindow(hwndMain, SW_SHOWNORMAL);
  if (nid.uID != IDI_SHELLNOTIFY) {
    return;  // already created;
  }
  Shell_NotifyIcon(NIM_DELETE, &nid);
  nid.uID = 0;
}

//////////////////////////////////////////////////////////////////////////////
static void add_notify_icon(void)
{
  ShowWindow(hwndMain, SW_HIDE);
  if (nid.uID == IDI_SHELLNOTIFY) {
    return;  // already created;
  }
  nid.uID = IDI_SHELLNOTIFY;
  Shell_NotifyIcon(NIM_ADD, &nid);
}

//////////////////////////////////////////////////////////////////////////////
static void mainframeOnCommand(HWND hwnd, int id, HWND hwndCtl, UINT cNotify)
{
  switch (id) {
    case ID_ITUNESWEB_EXIT:
      del_notify_icon();
      PostQuitMessage(0);
      return;
    case ID_ITUNESWEB_STATUS:
      ShellExecute(NULL, "open", "http://localhost:8080", NULL, NULL,
     		   SW_SHOWNORMAL);
      return;
    case ID_ITUNESWEB_HIDE:
      add_notify_icon();
      return;
    case ID_ITUNESWEB_ABOUT:
      DialogBox(GetWindowInstance(hwndMain), MAKEINTRESOURCE(DLG_ABOUT),
     		hwndMain, (DLGPROC)DlgProc);
      return;
  }
}

//////////////////////////////////////////////////////////////////////////////
static void mainframeOnDestroy(HWND hwnd)
{
  PostQuitMessage(0);
}

//////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK mainframeWndProc(HWND hwnd, UINT message, WPARAM wParam,
                                  LPARAM lParam)
{
  if (wParam == IDI_SHELLNOTIFY) {
    switch (lParam) {
      case WM_LBUTTONDBLCLK:
	del_notify_icon();
	return 0;
      case WM_CONTEXTMENU:
      case WM_RBUTTONDOWN: {
	HMENU hmenu = GetSubMenu(GetMenu(hwndMain), 0);
	POINT pt;
	GetCursorPos(&pt);
	SetForegroundWindow(hwnd);  // so clicks outside popup are handled
        TrackPopupMenuEx(hmenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN |
			 TPM_RIGHTALIGN, pt.x, pt.y, hwndMain, NULL);
        return 0;
      }
    }
  }
  switch (message) {
    HANDLE_MSG(hwnd, WM_COMMAND, mainframeOnCommand);
    HANDLE_MSG(hwnd, WM_DESTROY, mainframeOnDestroy);
    case WM_CLOSE:
      add_notify_icon();
      return 0;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
static ATOM register_window_class(HINSTANCE hinst)
{
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = mainframeWndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hinst;
  wcex.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(IDI_ICON1));
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = MAKEINTRESOURCE(IDR_MAINFRAME);
  wcex.lpszClassName = AppClassName;
  wcex.hIconSm = LoadIcon(hinst, MAKEINTRESOURCE(IDI_ICON2));
  return RegisterClassEx(&wcex);
}

//////////////////////////////////////////////////////////////////////////////
DWORD WINAPI GuiThreadProc(LPVOID lpParameter)
{
  HINSTANCE hinst = *(HINSTANCE *)lpParameter;
  HACCEL haccel;
  HWND hwndt;	// tmp for accelerator rewrite
  MSG msg;

  // create main application window
  hwndMain = CreateWindowEx(0, AppClassName, AppTitle, WS_OVERLAPPEDWINDOW,
	CW_USEDEFAULT, CW_USEDEFAULT, 224, 128, NULL, NULL, hinst, NULL);
  if (!hwndMain) {
    cleanup_exit(1);  // failed to create main window
  }

  ShowWindow(hwndMain, SW_SHOW);
  haccel = LoadAccelerators(hinst, MAKEINTRESOURCE(IDR_MAINFRAME));

  // prepare notify icon data
  ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
  nid.cbSize = NOTIFYICONDATA_V2_SIZE;
  nid.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP|NIF_INFO;
  nid.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(IDI_ICON2));
  nid.hWnd = hwndMain;
  nid.uCallbackMessage = IDI_SHELLNOTIFY;
  LoadString(hinst, IDS_INIT_ICON_TEXT, nid.szTip, sizeof(nid.szTip));
  LoadString(hinst, IDS_INIT_ICON_TEXT, nid.szInfo, sizeof(nid.szInfo));
  nid.uTimeout = 12000;  // timeout in milliseconds, 10000 - 30000
  LoadString(hinst, IDS_APP_TITLE, nid.szInfoTitle, sizeof(nid.szInfoTitle));
  nid.dwInfoFlags = NIIF_USER;

  // main gui event loop
  while (GetMessage(&msg, NULL, 0, 0)) {
    hwndt = msg.hwnd;
    if (!TranslateAccelerator(hwndt, haccel, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  del_notify_icon();
  cleanup_exit(0);
  return 0;  // never reached
}

//////////////////////////////////////////////////////////////////////////////
static BOOL init_instance(HINSTANCE hinst)
{
  HWND hwnd;

  LoadString(hinst, IDR_MAINFRAME, AppClassName, sizeof(AppClassName));
  LoadString(hinst, IDS_APP_TITLE, AppTitle, sizeof(AppTitle));
  hwnd = FindWindow(AppClassName, NULL);
  // if app already running (class exists), show existing window, exit this
  if (hwnd) {
    if (IsIconic(hwnd)) {
      ShowWindow(hwnd, SW_RESTORE);
    }
    if (!IsWindowVisible(hwnd)) {
      // fake a double click of the shell notify icon
      SendMessage(hwnd, WM_LBUTTONDBLCLK, IDI_SHELLNOTIFY, WM_LBUTTONDBLCLK);
    }
    SetForegroundWindow(hwnd);
    return FALSE;  // force exit of new invocation of app
  }
  if (!register_window_class(hinst)) {
    return FALSE;  // failed to register app window class
  }
  //InitCommonControls();   // make common controls available
  return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
		   LPSTR lpCmdLine, int nCmdShow)
{
  int i, ok = 1;
  string itd, pkt;
  __time64_t ltime;

  if (!init_instance(hInstance)) {
    return FALSE;
  }
  init_server();
  init_iTunes();

  if (CreateThread(NULL, 0, GuiThreadProc, &hInstance, 0, NULL) == NULL) {
    MessageBox(NULL, TEXT("CreateThread Failed, Exiting..."),
	       TEXT("CreateThread Error"), MB_OK); 
    cleanup_exit(1);
  }
  while (ok) {
    FD_ZERO(&readfs);
    for (i = 0; i < sockfdmax; i++) {
      FD_SET(sockfd[i], &readfs);
    }
    if (select(FD_SETSIZE, &readfs, NULL, NULL, NULL) > 0) {
      for (i = 0; i < sockfdmax; i++) {
	if (FD_ISSET(sockfd[i], &readfs)) {
	  if (i == 0) {
	    sockfd[sockfdmax] = AcceptConnect();
	    if (((reqbuf[sockfdmax] = (char *)malloc(MAXREQB)) == NULL) ||
	        (sockfdmax >= MAXCONN)) {
	      // out of memory or max connections reached
	      send(sockfd[sockfdmax],
		   "HTTP/1.1 503 Service Unavailable\r\n", 34, 0);
	      closesocket(sockfd[sockfdmax]);
	    } else {
	      reqlen[sockfdmax] = 0;	// no data yet receivd for connection
	      sockfdmax++;
	    }
	  } else {
	    if (!ReadInput(i)) {
	      RemoveConnect(i);
	      break;	// setup readfs again, socket list is now changed...
	    }
	    if (netlen + reqlen[i] >= MAXREQB) {
	      // get to the point already, evil client?
	      send(sockfd[i], "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	      RemoveConnect(i);
	      break;	// setup readfs again, socket list is now changed...
	    }
	    // append packet to everything received so far...
	    memcpy(&reqbuf[i][reqlen[i]], netbuf, netlen);
	    reqlen[i] += netlen;
	    reqbuf[i][reqlen[i]] = '\0';
	    char *tmp = strstr(reqbuf[i], "\r\n\r\n"), savechar;
	    if (!tmp)
	      continue;	// wait for complete header with terminal blank line

	    char *contentlen = strstr(reqbuf[i], "\nContent-Length:");
	    if (contentlen) {
	      size_t bodylen = atoi(contentlen + 16);
	      if (strlen(tmp) - 4 < bodylen)
		continue;	// wait for complete body
	      savechar = tmp[4 + bodylen];
	      tmp[4 + bodylen] = '\0';	// nul terminate body
	    } else {
	      savechar = tmp[4];
	      tmp[4] = '\0';	// nul terminate request
	    }
cout << inet_ntoa(fromhost[i]) << ": " << reqbuf[i] << endl;
  	    int keepalive = (strstr(reqbuf[i], " HTTP/1.0") == NULL) &&
			    (strstr(reqbuf[i], "Connection: close") == NULL);
	    int len = strlen(reqbuf[i]);
	    // send completed request to itunes handler
	    itd = get_iTunes(reqbuf[i], len);
	    // restore character overwritten with nul
	    reqbuf[i][len] = savechar;
	    // move following chars back to start of request buffer
	    if (len < reqlen[i])
	      memmove(reqbuf[i], &reqbuf[i][len], reqlen[i] - len);
	    // setup accounting for future (keep-alive) request
	    reqlen[i] = reqlen[i] - len;
	    reqbuf[i][reqlen[i]] = '\0';
	    char buf[32];
	    sprintf(buf, "%d", itd.length());
	    _time64(&ltime);
	    pkt = "HTTP/1.1 200 OK\r\nDate: ";
	    pkt += _ctime64(&ltime);
	    pkt += "Server: iTunesWeb/1.0.0.4 (Win32) (Windows XP)\r\n";
	    pkt += "Cache-Control: no-cache\r\nPragma: no-cache\r\n";
	    pkt += "Expires: -1\r\nLast-Modified: ";
	    pkt += _ctime64(&ltime);
	    pkt += "Content-Length: ";
	    pkt += buf;
	    if (keepalive) {
	      pkt += "\r\nKeep-Alive: timeout=900, max=0";
	      pkt += "\r\nConnection: Keep-Alive\r\n";
	    } else {
	      pkt += "\r\nConnection: close\r\n";
	    }
	    pkt += "Content-Type: text/html; charset=UTF-8\r\n\r\n";
	    pkt += itd;
	    send(sockfd[i], pkt.c_str(), pkt.length(), 0);
	    if (!keepalive) {
	      RemoveConnect(i);
	      break;	// setup readfs again, socket list is now changed...
	    }
	  }
	}
      }
    } else {
      ok = 0;
    }
  }
  cleanup_exit(0);
  return 1;	// never reached
}

//////////////////////////////////////////////////////////////////////////////
