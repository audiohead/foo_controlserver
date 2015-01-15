//  $Id: main.cpp 75 2006-12-04 21:51:48Z jcpolos $
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

#include "controlserver.h"
#include "chandlecallbackrun.h"
#include "../foobar2000_sdk/foobar2000/SDK/foobar2000.h"
#include "../foobar2000_sdk/pfc/pfc.h"
#include "resource.h"

// variable to store server port etc
static GUID guid1 = { 0xdfc08ea6, 0xb17f, 0x494d, 0xbb, 0xb3, 0x6c, 0x8f, 0xa1, 0x27, 0xa7, 0xcf };
static GUID guid2 = { 0xec468df8, 0x03f7, 0x45c1, 0x86, 0xc9, 0xde, 0x3a, 0x09, 0x53, 0xb4, 0x10 };
static GUID guid3 = { 0x4a8db709, 0x2977, 0x4d31, 0x92, 0x92, 0x54, 0x18, 0xa2, 0xf1, 0x41, 0x22 };
static GUID guid4 = { 0x3d4beb75, 0x13fa, 0x4e6b, 0x8b, 0xdb, 0xaa, 0x10, 0xf0, 0x5d, 0x81, 0x7d };
static GUID guid5 = { 0x78b38c5b, 0x4efa, 0x4b3c, 0xae, 0x9c, 0xde, 0xd1, 0xf8, 0x71, 0x63, 0x42 };
static GUID guid6 = { 0x43ccc0d7, 0xbeff, 0x461e, 0xad, 0xa2, 0x12, 0xa6, 0xe6, 0x2c, 0x48, 0xf5 };
static GUID guid7 = { 0x6d69e487, 0x280b, 0x445e, 0x92, 0xe4, 0xa0, 0x91, 0x41, 0x2f, 0x11, 0xa1 };
static GUID guid8 = { 0x0f172c1e, 0x5b11, 0x4462, 0xb8, 0xb4, 0x66, 0xb8, 0xbe, 0x53, 0x24, 0x4b };
static cfg_int cfg_server_port(guid1, 3333);
static cfg_int cfg_max_clients(guid2, 4);
static cfg_string cfg_fields(guid3, "%length_seconds%|%codec%|%bitrate%|$if(%album artist%,%album artist%,%artist%)|%album%|%date%|%genre%|%tracknumber%|%title%");
static cfg_string cfg_connection_mask(guid4, "127.0.0.1,*.*.*.*");
static cfg_string cfg_delimit(guid5,"|");
static cfg_int cfg_server_enabled(guid6,1);
static cfg_int cfg_utf8_output(guid7,1);
static cfg_int cfg_prevent_close(guid8,1);

// this will handle some events we are interested in
class play_callback_controlserver : public play_callback_static
{
public:
    virtual void FB2KAPI on_playback_starting(play_control::t_track_command p_command,bool p_paused) { };

    virtual void FB2KAPI on_playback_new_track(metadb_handle_ptr p_track)
    {
        static_api_ptr_t<main_thread_callback_manager> cm;

        cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::trackupdatefromptr, p_track));
    };

    virtual void FB2KAPI on_playback_stop(play_control::t_stop_reason p_reason)
    {
        // notify clients playback stopped if we stopped for real
        if (p_reason != playback_control::stop_reason_starting_another)
        {
            static_api_ptr_t<main_thread_callback_manager> cm;

            cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::trackupdateonconnect, (SOCKET)NULL));
        }
    };

    virtual void FB2KAPI on_playback_seek(double p_time)
    {
        static_api_ptr_t<main_thread_callback_manager> cm;

        cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::trackupdateonconnect, (SOCKET)NULL));
    };

    virtual void FB2KAPI on_playback_pause(bool p_state)
    {
        static_api_ptr_t<main_thread_callback_manager> cm;

        cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::trackupdateonconnect, (SOCKET)NULL));
    };

    // prolly dont want to handle this
    virtual void FB2KAPI on_playback_edited(metadb_handle_ptr p_track) { };

    // vbr change etc
    virtual void FB2KAPI on_playback_dynamic_info(const file_info & p_info) { };

    // stream file info changed perhaps
    // should probably handle this in the future
    virtual void FB2KAPI on_playback_dynamic_info_track(const file_info & p_info) { };

    // not handling
    virtual void FB2KAPI on_playback_time(double p_time) { };

    // handle volume changes
    virtual void FB2KAPI on_volume_change(float p_new_val)
    {
        static_api_ptr_t<main_thread_callback_manager> cm;

        cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::volumechange, (float)p_new_val));
    };

    // events we are interested in
    unsigned get_flags()
    {
        return flag_on_playback_new_track | flag_on_playback_stop |
               flag_on_playback_pause | flag_on_volume_change |
               flag_on_playback_seek;
    };

   FB2K_MAKE_SERVICE_INTERFACE(play_callback_controlserver, play_callback_static);
};

DECLARE_CLASS_GUID(play_callback_controlserver, 0xff3eb8d7, 0x97dd, 0x48b3, 0x80, 0xae, 0x15, 0x5b, 0x00, 0x99, 0x0c, 0x6d);

// used on intializtion of foobar to initalize plugin
// we create the server thread with this and kill it on close
class initquit_controlserver : public initquit
{
private:
    static HANDLE h_thread;
    static HANDLE endEvent;

    static DWORD WINAPI g_threadfunc(void* endEvent)
    {
        controlserver::start(cfg_server_port, static_cast<HANDLE>(endEvent));
        return 0;
    };

    inline static HANDLE create_thread(HANDLE event)
    {
        DWORD id;

        assert(core_api::is_main_thread());

        return h_thread = CreateThread(0, 0, g_threadfunc, event, 0, &id);
    };

    static void stop_thread()
    {
        assert(core_api::is_main_thread());

        if (h_thread)
        {
            WaitForSingleObject(h_thread, INFINITE);
            CloseHandle(h_thread);
            CloseHandle(endEvent);
            h_thread = 0;
            endEvent = 0;
        }
    };

public:
    static void do_init()
    {
        // create end event
        endEvent =  CreateEvent(NULL, TRUE, FALSE, NULL);

        // set these vars.. this thread safe or need mutexes?:p
        // should prolly have a struct for this instead as well
        controlserver::handleFieldsChange(cfg_fields);
        controlserver::handleDelimitStringChange(cfg_delimit);
        controlserver::handleUtf8OutputChange(cfg_utf8_output);
        controlserver::handleMaxClientsChange(cfg_max_clients);
        controlserver::handleConnectionMaskChange(cfg_connection_mask);
        controlserver::handlePreventCloseChange(cfg_prevent_close);

        // create our server! this will spawn additional threads
        // to handle connections to the server
        create_thread(endEvent);
    };
    static void do_quit()
    {
        controlserver::m_currTrackPtr.release();
        controlserver::m_prevTrackPtr.release();

        // create event to stop the init
        SetEvent(endEvent);
        stop_thread();
    };
    virtual void on_init()
    {
        if (cfg_server_enabled)
        {
            do_init();
        }
    };

    virtual void on_quit()
    {
        do_quit();
    };

    FB2K_MAKE_SERVICE_INTERFACE(initquit_controlserver, initquit);
};

DECLARE_CLASS_GUID(initquit_controlserver,0xf888b7e9, 0xf120, 0x41de, 0x84, 0xd2, 0xeb, 0x40, 0xcb, 0xe3, 0xd5, 0xcb);

// set these to null initially
HANDLE initquit_controlserver::endEvent = NULL;
HANDLE initquit_controlserver::h_thread = NULL;

// class for configuration form config, updates etc
class config_controlserver : public preferences_page
{
public:

    virtual HWND create(HWND parent)
    {
        return uCreateDialog(IDD_CS_CONFIGVIEW, parent, ConfigProc);
    };

    virtual const char * get_name()
    {
        return "Control Server";
    };

    virtual GUID get_guid()
    {
        return pfc::GUID_from_text("foo_controlserver:_config_controlserver_prefs");
    }

    virtual GUID get_parent_guid()
    {
        return preferences_page::guid_tools;
    };

    virtual bool reset_query()
    {
        return true;
    };

    virtual void reset()
    {
        // these should match the var defaults at top of this file
        // could make constants for this I suppose
        cfg_fields = "%length_seconds%|%codec%|%bitrate%|$if(%album artist%,%album artist%,%artist%)|%album%|%date%|%genre%|%tracknumber%|%title%";
        cfg_delimit = "|";
        cfg_utf8_output = 1;
        cfg_prevent_close = 1;
        cfg_max_clients = 4;
        cfg_connection_mask = "127.0.0.1,*.*.*.*";

        // notify control server of these changes
        controlserver::handleFieldsChange(cfg_fields);
        controlserver::handleDelimitStringChange(cfg_delimit);
        controlserver::handleUtf8OutputChange(cfg_utf8_output);
        controlserver::handlePreventCloseChange(cfg_prevent_close);

        if (!cfg_server_enabled || cfg_server_port!=3333)
        {
            cfg_server_port = 3333;
            cfg_server_enabled = 1;
            controlserver::handleMaxClientsChange(cfg_max_clients);
            controlserver::handleConnectionMaskChange(cfg_connection_mask);

            // restart server
            initquit_controlserver::do_quit();
            initquit_controlserver::do_init();
        }
    };

    static BOOL CALLBACK ConfigProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch(msg)
        {
            case WM_INITDIALOG:
            {
                pfc::string8 temp;

                temp << cfg_server_port;
                // display the server port in the edit box
                uSetDlgItemText(wnd, IDC_SERVERPORT, temp);

                temp.reset();
                temp << cfg_max_clients;
                uSetDlgItemText(wnd, IDC_MAXCLIENTS, temp);

                // set check box
                uSendDlgItemMessage(wnd, IDC_SERVERENABLED, BM_SETCHECK, cfg_server_enabled, 0);

                // set check box
                uSendDlgItemMessage(wnd, IDC_UTF8OUTPUT, BM_SETCHECK, cfg_utf8_output, 0);

                // prevent close check
                uSendDlgItemMessage(wnd, IDC_PREVENTCLOSE, BM_SETCHECK, cfg_prevent_close, 0);

                // display metafields
                uSetDlgItemText(wnd, IDC_METAFIELDS, cfg_fields);

                // display metafields
                uSetDlgItemText(wnd, IDC_DELIMIT, cfg_delimit);

                // display connection mask
                uSetDlgItemText(wnd, IDC_CONNECTIONMASK, cfg_connection_mask);
            }
            break;

            case WM_COMMAND:
            {
                switch(LOWORD(wp))
                {
                    case IDC_SERVERPORT:
                    {
                        if (HIWORD(wp)==EN_UPDATE)
                        {
                            t_size i = 3333;
                            pfc::string8 temp;

                            // get the value from the edit box
                            uGetDlgItemText(wnd, IDC_SERVERPORT, temp);

                            // convert to int
                            if (!controlserver::convertStrToI(temp, i))
                            {
                                i = 3333;
                            }

                            // make sure port is valid range
                            if (i<1 || i>65535)
                            {
                                i = 3333;
                            }

                            // store the new server port into the configuration variable
                            cfg_server_port = i;
                        }
                    }
                    break;
                    case IDC_MAXCLIENTS:
                    {
                        if (HIWORD(wp)==EN_UPDATE)
                        {
                            t_size i = 4;
                            pfc::string8 temp;

                            // get the value from the edit box
                            uGetDlgItemText(wnd, IDC_MAXCLIENTS, temp);

                            // convert to int
                            if (!controlserver::convertStrToI(temp, i))
                            {
                                i = 4;
                            }

                            // make sure port is valid range
                            if (i<1 || i>65535)
                            {
                                i = 4;
                            }

                            // store the new server port into the configuration variable
                            cfg_max_clients = i;
                        }
                    }
                    break;
                    case IDC_SERVERENABLED:
                    {
                        cfg_server_enabled = uSendMessage(reinterpret_cast<HWND>(lp), BM_GETCHECK, 0, 0);

                        if (cfg_server_enabled)
                        {
                            initquit_controlserver::do_init();
                        }
                        else
                        {
                            initquit_controlserver::do_quit();
                        }
                    }
                    break;
                    case IDC_UTF8OUTPUT:
                    {
                        pfc::string8 str;
                        pfc::string8 cleanData;

                        cfg_utf8_output = uSendMessage(reinterpret_cast<HWND>(lp), BM_GETCHECK, 0, 0);
                        controlserver::handleUtf8OutputChange(cfg_utf8_output);

                        controlserver::generateTrackString(controlserver::m_currTrackPtr, str, false);

                        // strip off trailing delimiter and linefeeds
                        str.truncate(str.length()-3);

                        if (!cfg_utf8_output)
                        {
                            controlserver::convertFromWide(str, cleanData);
                        }
                        else
                        {
                            cleanData = str;
                        }
                        uSetDlgItemText(wnd, IDC_PREVIEW, cleanData);
                    }
                    break;
                    case IDC_PREVENTCLOSE:
                    {
                        cfg_prevent_close = uSendMessage(reinterpret_cast<HWND>(lp), BM_GETCHECK, 0, 0);
                        controlserver::handlePreventCloseChange(cfg_prevent_close);
                    }
                    break;
                    case IDC_METAFIELDS:
                    {
                        if (HIWORD(wp)==EN_UPDATE)
                        {
                            pfc::string8 str;
                            pfc::string8 cleanData;

                            // store the metafields into the configuration variable
                            uGetDlgItemText(wnd, IDC_METAFIELDS, cfg_fields);
                            controlserver::handleFieldsChange(cfg_fields);

                            controlserver::generateTrackString(controlserver::m_currTrackPtr, str, false);
                            // strip off trailing delimiter and linefeeds
                            str.truncate(str.length()-3);
    
                            if (!cfg_utf8_output)
                            {
                                controlserver::convertFromWide(str, cleanData);
                            }
                            else
                            {
                                cleanData = str;
                            }
                            uSetDlgItemText(wnd, IDC_PREVIEW, cleanData);
                        }
                    }
                    break;
                    case IDC_CONNECTIONMASK:
                    {
                        if (HIWORD(wp)==EN_UPDATE)
                        {
                            // store the metafields into the configuration variable
                            uGetDlgItemText(wnd, IDC_CONNECTIONMASK, cfg_connection_mask);
                        }
                    }
                    break;
                    case IDC_DELIMIT:
                    {
                        if (HIWORD(wp)==EN_UPDATE)
                        {
                            // store the new delimit char into the configuration variable
                            uGetDlgItemText(wnd, IDC_DELIMIT, cfg_delimit);
                            controlserver::handleDelimitStringChange(cfg_delimit);
                        }
                    }
                    break;
                }
            }
            break;

            case WM_DESTROY:
            {

            }
            break;

        }
        return 0;
    };

    FB2K_MAKE_SERVICE_INTERFACE(config_controlserver, preferences_page);
};

DECLARE_CLASS_GUID(config_controlserver,0x228ea2b9, 0x6b0b, 0x4dec, 0xb9, 0x83, 0x48, 0xb9, 0x1c, 0x54, 0x26, 0x15);

static initquit_factory_t<initquit_controlserver> foo;
static preferences_page_factory_t<config_controlserver> foo2;
static play_callback_static_factory_t<play_callback_controlserver> foo3;

DECLARE_COMPONENT_VERSION("Control Server", controlserver::m_versionNumber,
                          "foo_controlserver - server plugin to control foobar2000 over TCP/IP\n"
                          "Copyright (C) 2003, 2004, 2005, 2006  Jason Poloski\n"
                          "\n"
                          "This program is free software; you can redistribute it and/or\n"
                          "modify it under the terms of the GNU General Public License\n"
                          "as published by the Free Software Foundation; either version 2\n"
                          "of the License, or (at your option) any later version.\n"
                          "\n"
                          "This program is distributed in the hope that it will be useful,\n"
                          "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                          "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
                          "GNU General Public License for more details.\n"
                          "\n"
                          "You should have received a copy of the GNU General Public License\n"
                          "along with this program; if not, write to the Free Software\n"
                          "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.");
