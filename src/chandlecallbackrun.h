//  $Id: chandlecallbackrun.h 75 2006-12-04 21:51:48Z jcpolos $
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

#ifndef INC_CHANDLERCALLBACKRUN_H
#define INC_CHANDLERCALLBACKRUN_H

#include "controlserver.h"
#include "../foobar2000_sdk/foobar2000/SDK/foobar2000.h"

class CHandleCallbackRun : public main_thread_callback
{
public:
    enum CallbackType { trackupdatefromptr, trackupdateonconnect, play, listinfo, \
                        search, order, trackinfo, inittrackers, volumeset, list, \
                        queue, next, prev, rand, stop, pause, seek, volumechange, listmod };

    CHandleCallbackRun(CallbackType);
    CHandleCallbackRun(CallbackType, SOCKET);
    CHandleCallbackRun(CallbackType, SOCKET, pfc::string8);
    CHandleCallbackRun(CallbackType, metadb_handle_ptr);
    CHandleCallbackRun(CallbackType, float);
    virtual void callback_run();

private:
    CallbackType m_ctype;
    SOCKET m_sock;
    pfc::string8 m_str;
    metadb_handle_ptr m_trackPtr;
    float m_vol;
};

#endif  // INC_CHANDLERCALLBACKRUN_H
