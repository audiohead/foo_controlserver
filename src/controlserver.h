//  $Id: controlserver.h 75 2006-12-04 21:51:48Z jcpolos $
/*
 * foo_controlserver - server plugin to control foobar2000 over TCP/IP
 * Copyright (C) 2003, 2004, 2005, 2006  Jason Poloski
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef INC_CONTROLSERVER_H
#define INC_CONTROLSERVER_H

#include <winsock2.h>
#include <vector>
#include "../foobar2000_sdk/foobar2000/SDK/foobar2000.h"
#include "../foobar2000_sdk/pfc/pfc.h"

class controlserver
{
public:
    static std::vector<SOCKET> m_vclientSockets;
    static HANDLE m_WSAendEvent;
    static unsigned int const m_bufferSize;
    static bool m_muted;
    static pfc::string8 m_metafields;
    static pfc::string8 m_delimit;
    static t_size m_maxClients;
    static pfc::string8 const m_versionNumber;
    static t_size m_currTrackIndex;
    static t_size m_prevTrackIndex;
    static t_size m_currPlaylistIndex;
    static metadb_handle_ptr m_currTrackPtr;
    static metadb_handle_ptr m_prevTrackPtr;

    controlserver();
    ~controlserver();

    static void handleVolumeSetCommand(SOCKET, pfc::string8);
    static void handleListCommand(SOCKET, pfc::string8);
    static void handleNextCommand(SOCKET, pfc::string8);
    static void handlePrevCommand(SOCKET, pfc::string8);
    static void handleStopCommand(SOCKET);
    static void handlePauseCommand(SOCKET);
    static void handleHelpCommand(SOCKET, bool);
    static void handleServerInfoCommand(SOCKET);
    static void handlePlayCommand(SOCKET, pfc::string8);
    static void handleTrackInfoCommand(SOCKET);
    static void handleListInfoCommand(SOCKET, pfc::string8);
    static void handleOrderCommand(SOCKET, pfc::string8);
    static void handleRandomCommand(SOCKET, pfc::string8);
    static void handleSearchCommand(SOCKET, pfc::string8);
    static void handleQueueTrackCommand(SOCKET, pfc::string8);
    static void handleSeekTrackCommand(SOCKET, pfc::string8);
	static void handleListModCommand(SOCKET, pfc::string8);

    static void start(int, HANDLE);
    static void handleDelimitStringChange(pfc::string8 const&); // thread safe?
    static void handleFieldsChange(pfc::string8 const&);        // thread safe?
    static void handleDelimitEmptyChange(t_size);
    static void handleUtf8OutputChange(t_size);
    static void handlePreventCloseChange(t_size);
    static void handleMaxClientsChange(t_size);
    static void handleConnectionMaskChange(pfc::string8 const&);
    static void handleVolumeChange(float);
    static void handleTrackUpdateOnConnect(SOCKET);
    static void handleTrackUpdateFromPtr(metadb_handle_ptr p_track);
    static void initTrackers();
    static bool convertStrToI(char const*, t_size&);
    static bool convertStrToF(char const*, double&);
    static void generateTrackString(metadb_handle_ptr, pfc::string8&, bool);
    static bool convertFromWide(pfc::string8 const&, pfc::string8&);

private:
    static t_size m_utf8Output;
    static t_size m_preventClose;
    static t_size m_delimitEmpty;
    static pfc::string8 m_connectionMask;
    static void shutdownClient(SOCKET);
    static DWORD WINAPI ClientThread(void*);
    static void calculateTrackStateHeader(pfc::string8&, int);
    static void generateSearchString(metadb_handle_ptr, pfc::string8&, pfc::string8&);
    static pfc::string8 const calculateSocketAddress(SOCKET);
    static bool sendData(SOCKET, pfc::string8 const&);
    static bool validateConnectionMask(SOCKADDR_IN);
    static bool validateFuzzyMatch(pfc::string8, pfc::string8);
};

#endif  // INC_CONTROLSERVER_H
