//  $Id: chandlecallbackrun.cpp 75 2006-12-04 21:51:48Z jcpolos $
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

#include "chandlecallbackrun.h"

CHandleCallbackRun::CHandleCallbackRun(CallbackType t)
    : m_ctype(t),
    m_sock(NULL),
    m_trackPtr(NULL),
    m_str(),
    m_vol(0),
    main_thread_callback()
{

}

CHandleCallbackRun::CHandleCallbackRun(CallbackType t, SOCKET sock)
    : m_ctype(t),
    m_sock(sock),
    m_trackPtr(NULL),
    m_str(),
    m_vol(0),
    main_thread_callback()
{

}

CHandleCallbackRun::CHandleCallbackRun(CallbackType t, SOCKET sock, pfc::string8 str)
    : m_ctype(t),
    m_sock(sock),
    m_trackPtr(NULL),
    m_str(str),
    m_vol(0),
    main_thread_callback()
{

}

CHandleCallbackRun::CHandleCallbackRun(CallbackType t, metadb_handle_ptr track)
    : m_ctype(t),
    m_sock(NULL),
    m_trackPtr(track),
    m_str(),
    m_vol(0),
    main_thread_callback()
{

}

CHandleCallbackRun::CHandleCallbackRun(CallbackType t, float vol)
    : m_ctype(t),
    m_sock(NULL),
    m_trackPtr(NULL),
    m_str(),
    m_vol(vol),
    main_thread_callback()
{

}

void
CHandleCallbackRun::callback_run()
{
    switch (m_ctype)
    {
    case trackupdatefromptr:
        controlserver::handleTrackUpdateFromPtr(m_trackPtr);
        break;
    case trackupdateonconnect:
        controlserver::handleTrackUpdateOnConnect(m_sock);
        break;
    case play:
        controlserver::handlePlayCommand(m_sock, m_str);
        break;
    case listinfo:
        controlserver::handleListInfoCommand(m_sock, m_str);
        break;
    case search:
        controlserver::handleSearchCommand(m_sock, m_str);
        break;
    case order:
        controlserver::handleOrderCommand(m_sock, m_str);
        break;
    case trackinfo:
        controlserver::handleTrackInfoCommand(m_sock);
        break;
    case inittrackers:
        controlserver::initTrackers();
        break;
    case volumeset:
        controlserver::handleVolumeSetCommand(m_sock, m_str);
        break;
    case list:
        controlserver::handleListCommand(m_sock, m_str);
        break;
    case queue:
        controlserver::handleQueueTrackCommand(m_sock, m_str);
        break;
    case next:
        controlserver::handleNextCommand(m_sock, m_str);
        break;
    case prev:
        controlserver::handlePrevCommand(m_sock, m_str);
        break;
    case rand:
        controlserver::handleRandomCommand(m_sock, m_str);
        break;
    case stop:
        controlserver::handleStopCommand(m_sock);
        break;
    case pause:
        controlserver::handlePauseCommand(m_sock);
        break;
    case seek:
        controlserver::handleSeekTrackCommand(m_sock, m_str);
        break;
    case volumechange:
        controlserver::handleVolumeChange(m_vol);
        break;
    default:
        break;
    };
}

