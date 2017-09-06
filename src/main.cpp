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

/*
 * Version 1.1.3 Nov 2016 - Walter Hartman
 *   - Rebuilt using VS2015 Community and the new foobar2000 SDK version 2015-08-03
 *   - Updated the preferences dialog to use the new foobar2000 API's
 *   - Added IP address display of local PC to preferences dialog
 *   - Added get album art command, returns image as a base64 string
 *   - Added media library search command
 *   - Fixed some bugs related to the 'now playing' track
 *
 *   This version of foo_controlserver component was built in the free version of
 *   Microsoft Visual Studio Community 2015 in C++, and also requires installation of
 *   the Windows Template Library WTL and the Microsoft Foundation Class Library MFC
 *   in your VS2015 setup. You will also need the Foobar2000 SDK to do this build.
 * 
 *   1) In VS2015, use nuget to install the WTL (Windows Template Library) -- needed
 *   for the Foobar2000 ATLHelpers library and to build the new preferences screen.
 *
 *   From VS2015, select Tools>NuGet Package Manager>Manager Nuget Packages for Solutions,
 *   then from that dialog, select 'Browse' and search for WTL.
 *
 *   This build was built using WTL version 9.1.0
 *
 *   2) Also make sure the MFC libraries were installed in your VS2015 Community setup. 
 *   That option is available when doing a 'custom' install under :
 *
 *   Programming languages> Visual C++ language > Microsoft Foundation Classes for C++
 *
 *   If the MFC classes were not installed in your initial VS2015 setup, you can modify
 *   your Visual Studio feature set afterwards by going to :
 *
 *   Control panel>Programs>Programs and Features>Microsoft Visual Studio Community 2015>Change
 * 
 *   3) Download the Foobar2000 SDK from http://www.foobar2000.org/SDK
 *   Version SDK 2015-08-03 was used for this build.
 *
 *   Unzip that SDK into a work directory. Place the 'foo_controlserver' folder 
 *   for this project into that work directory at the same level as the 
 *   Foobar2000 SDK, shared, ATLHelpers directories as Follows:
 * 
 *   ./SDK-2015-08-03/foobar2000/
 *                              ./SDK
 *                              ./shared
 *                              ./ATLHelpers
 *                              ./helpers
 *                              ./foo_sample
 *                              ./foo_controlserver   <---
 *
 *   Make sure you have the Foobar2000 SDK libraries built before attempting to build
 *   foo_controlserver since it depends on the SDK libraries.  
 *
 *   Relative directory paths for various includes used in foo_controlserver depend on
 *   the above directory structure.
 *
 *   4) ./foo_controlserver/src/foo_controlserver.sln is the VS2015 solution file for
 *   this project.
 *
 */

#include "controlserver.h"
#include "chandlecallbackrun.h"

#include "../../SDK/foobar2000.h"
#include "../../ATLHelpers/ATLHelpers.h"
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

t_size default_cfg_server_port = 3333;
t_size default_cfg_max_clients = 4;
pfc::string8 default_cfg_fields = "%length_seconds%|%codec%|%bitrate%|$if(%album artist%,%album artist%,%artist%)|%album%|%date%|%genre%|%tracknumber%|%title%";
pfc::string8 default_cfg_connection_masks = "127.0.0.1,*.*.*.*";
pfc::string8 default_cfg_delimiter = "|";
t_size default_cfg_server_enabled = 1;
t_size default_cfg_utf8_output = 1;
t_size default_cfg_prevent_close = 1;

static cfg_int cfg_server_port(guid1, default_cfg_server_port);
static cfg_int cfg_max_clients(guid2, default_cfg_max_clients);
static cfg_string cfg_fields(guid3, default_cfg_fields);
static cfg_string cfg_connection_masks(guid4, default_cfg_connection_masks);
static cfg_string cfg_delimiter(guid5, default_cfg_delimiter);
static cfg_int cfg_server_enabled(guid6, default_cfg_server_enabled);
static cfg_int cfg_utf8_output(guid7, default_cfg_utf8_output);
static cfg_int cfg_prevent_close(guid8, default_cfg_prevent_close);

class prefs_config {
public:
	t_size			server_port;
	t_size          max_clients;
	pfc::string8    connection_masks;
	pfc::string8    delimiter;
	pfc::string8	fields;
	t_size			server_enabled;
	t_size			utf8_output;
	t_size			prevent_close;

	prefs_config() { reset(); }
	prefs_config(const prefs_config &cfg) { copy(cfg); }
	prefs_config &operator = (const prefs_config &cfg) { copy(cfg); return *this; }
	bool operator == (const prefs_config &c);
	void copy(const prefs_config &cfg);
	void reset();
};

void prefs_config::reset()
{
	server_port = default_cfg_server_port;
	max_clients = default_cfg_max_clients;
	fields = default_cfg_fields;
	delimiter = default_cfg_delimiter;
	connection_masks = default_cfg_connection_masks;
	server_enabled = default_cfg_server_enabled;
	utf8_output = default_cfg_utf8_output;
	prevent_close = default_cfg_prevent_close;
}

bool prefs_config::operator == (const prefs_config &c)
{
	bool isDiff = ((c.server_port == server_port)
		&& (c.max_clients == max_clients)
		&& (c.connection_masks == connection_masks)
		&& (c.delimiter == delimiter)
		&& (c.fields == fields)
		&& (c.server_enabled == server_enabled)
		&& (c.utf8_output == utf8_output)
		&& (c.prevent_close == prevent_close));

	return isDiff;
}

void prefs_config::copy(const prefs_config &cfg)
{
	server_port = cfg.server_port;
	max_clients = cfg.max_clients;
	connection_masks = cfg.connection_masks;
	delimiter = cfg.delimiter;
	fields = cfg.fields;
	server_enabled = cfg.server_enabled;
	utf8_output = cfg.utf8_output;
	prevent_close = cfg.prevent_close;
}

prefs_config cfg;

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
        controlserver::handleDelimitStringChange(cfg_delimiter);
        controlserver::handleUtf8OutputChange(cfg_utf8_output);
        controlserver::handleMaxClientsChange(cfg_max_clients);
        controlserver::handleConnectionMaskChange(cfg_connection_masks);
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
class CMyPreferences : public CDialogImpl<CMyPreferences>, public preferences_page_instance
{
public:
	//Constructor - invoked by preferences_page_impl helpers - don't do Create() in here, preferences_page_impl does this for us
	CMyPreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

	//Note that we don't bother doing anything regarding destruction of our class.
	//The host ensures that our dialog is destroyed first, then the last reference to our preferences_page_instance object is released, causing our object to be deleted.

	//dialog resource ID
	enum { IDD = IDD_CS_CONFIGVIEW };
	// preferences_page_instance methods (not all of them - get_wnd() is supplied by preferences_page_impl helpers)
	t_uint32 get_state();
	void apply();
	void reset();

	//WTL message map
	BEGIN_MSG_MAP(CMyPreferences)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_SERVERPORT, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_MAXCLIENTS, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_SERVERENABLED, BN_CLICKED, OnEditChange)
		COMMAND_HANDLER_EX(IDC_UTF8OUTPUT, BN_CLICKED, OnEditChange)
		COMMAND_HANDLER_EX(IDC_PREVENTCLOSE, BN_CLICKED, OnEditChange)
		COMMAND_HANDLER_EX(IDC_METAFIELDS, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_CONNECTIONMASK, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_DELIMIT, EN_CHANGE, OnEditChange)
	END_MSG_MAP()

private:
	BOOL OnInitDialog(CWindow, LPARAM);
	void OnEditChange(UINT, int, CWindow);
	bool HasChanged();
	void OnChanged();
	void updateDialog();

	const preferences_page_callback::ptr m_callback;
	HWND hwnd = NULL;
	prefs_config cfg_new;

	void updateTrackFieldsPreview();

};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM) {

	hwnd = get_wnd();

	cfg_new.server_port = cfg_server_port;
	cfg_new.max_clients = cfg_max_clients;
	cfg_new.fields = cfg_fields;
	cfg_new.connection_masks = cfg_connection_masks;
	cfg_new.delimiter = cfg_delimiter;
	cfg_new.server_enabled = cfg_server_enabled;
	cfg_new.utf8_output = cfg_utf8_output;
	cfg_new.prevent_close = cfg_prevent_close;

	cfg.copy(cfg_new);

	updateDialog();

	return FALSE;
}

void CMyPreferences::OnEditChange(UINT, int nID, CWindow) 
{
	int i;

	switch (nID)
	{
	case IDC_SERVERPORT:
		cfg_new.server_port = GetDlgItemInt(IDC_SERVERPORT, NULL, FALSE);
		break;

	case IDC_MAXCLIENTS:
		i = GetDlgItemInt(IDC_MAXCLIENTS, NULL, FALSE);

		// make sure in valid range
		if (i < 1 || i>65535)
		{
			i = 4;
		}

		cfg_new.max_clients = i;
		break;

	case IDC_SERVERENABLED:
		cfg_new.server_enabled = uSendDlgItemMessage(IDC_SERVERENABLED, BM_GETCHECK, 0, 0);
		break;

	case IDC_UTF8OUTPUT:
		cfg_new.utf8_output = uSendDlgItemMessage(IDC_UTF8OUTPUT, BM_GETCHECK, 0, 0);
		break;

	case IDC_PREVENTCLOSE:
		cfg_new.prevent_close = uSendDlgItemMessage(IDC_PREVENTCLOSE, BM_GETCHECK, 0, 0);
		break;

	case IDC_METAFIELDS:
		uGetDlgItemText(hwnd, IDC_METAFIELDS, cfg_new.fields);
		break;

	case IDC_CONNECTIONMASK:
		uGetDlgItemText(hwnd, IDC_CONNECTIONMASK, cfg_new.connection_masks);
		break;

	case IDC_DELIMIT:
		uGetDlgItemText(hwnd, IDC_DELIMIT, cfg_new.delimiter);
		break;

	default: break;

	}

	OnChanged();
}

t_uint32 CMyPreferences::get_state() {
	t_uint32 state = preferences_state::resettable;
	if (HasChanged()) state |= preferences_state::changed;
	return state;
}

void CMyPreferences::updateDialog()
{
	pfc::string8 version = "Version : ";
	version << controlserver::m_versionNumber;
	uSetDlgItemText(hwnd, IDC_STATIC_VERSION, version);

	SetDlgItemInt(IDC_SERVERPORT, cfg_new.server_port, FALSE);
	SetDlgItemInt(IDC_MAXCLIENTS, cfg_new.max_clients, FALSE);
	CheckDlgButton(IDC_SERVERENABLED, cfg_new.server_enabled);
	CheckDlgButton(IDC_UTF8OUTPUT, cfg_new.utf8_output);
	CheckDlgButton(IDC_PREVENTCLOSE, cfg_new.prevent_close);

	uSetDlgItemText(hwnd, IDC_METAFIELDS, cfg_new.fields);
	uSetDlgItemText(hwnd, IDC_CONNECTIONMASK, cfg_new.connection_masks);
	uSetDlgItemText(hwnd, IDC_DELIMIT, cfg_new.delimiter);

	pfc::string8 msg;

	// if server disabled get Error 10093
	msg << controlserver::getLocalHostIP();
	uSetDlgItemText(hwnd, IDC_HOST_IP, msg);


	// If a track is loaded in foobar, update the preview of its fields
	updateTrackFieldsPreview();

}

void CMyPreferences::updateTrackFieldsPreview()
{
	pfc::string8 str;
	pfc::string8 cleanData;

	str = controlserver::generateNowPlayingTrackString();

	if (str.get_length() > 0)
	{
		// strip off trailing delimiter and linefeeds
		if (str.get_length() > 3) str.truncate(str.length() - 3);

		if (!cfg_new.utf8_output)
		{
			controlserver::convertFromWide(str, cleanData);
		}
		else
		{
			cleanData = str;
		}
	}
	else
	{
		cleanData = "no track loaded";
	}
	uSetDlgItemText(hwnd, IDC_PREVIEW, cleanData);

}

void CMyPreferences::reset() {
	cfg_new.reset();

	updateDialog();

	OnChanged();

}

void CMyPreferences::apply()
{
	// Update new original configuration
	cfg.copy(cfg_new);

	initquit_controlserver::do_quit();

	cfg_max_clients = cfg_new.max_clients;
	cfg_connection_masks = cfg_new.connection_masks;
	cfg_server_enabled = cfg_new.server_enabled;
	cfg_server_port = cfg_new.server_port;

	cfg_fields = cfg_new.fields;
	cfg_delimiter = cfg_new.delimiter;
	cfg_utf8_output = cfg_new.utf8_output;
	cfg_prevent_close = cfg_new.prevent_close;

	if (cfg_new.server_enabled)
	{
		// restart server
		initquit_controlserver::do_init();
	}

	updateDialog();
	OnChanged(); //our dialog content has not changed but the flags have - our currently shown values now match the settings so the apply button can be disabled
}

bool CMyPreferences::HasChanged()
{
	if (!(cfg == cfg_new))
		return true;
	else
		return false;
}

void CMyPreferences::OnChanged() {
	//tell the host that our state has changed to enable/disable the apply button appropriately.
	m_callback->on_state_changed();

}

class preferences_page_myimpl : public preferences_page_impl<CMyPreferences> {
	// preferences_page_impl<> helper deals with instantiation of our dialog; inherits from preferences_page_v3.
public:
	const char * get_name() { return "Control Server"; }
	GUID get_guid() {
	// This is our GUID.
	static const GUID guid = { 0xad5dada7, 0x9a45, 0x4c6c, 0xbc, 0x24, 0xf2, 0x25, 0x7f, 0x8f, 0x5a, 0xfc };
    return guid;
	}
	GUID get_parent_guid() { return guid_tools; }

};

static preferences_page_factory_t<preferences_page_myimpl> g_preferences_page_myimpl_factory;



static initquit_factory_t<initquit_controlserver> foo;

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
"Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.\n"
"\n"
"November 2016 Updated Version 1.1.3 :  Walter Hartman\n"
"\n"
"This updated version, along with its full source code, is released\n"
"as free software and under the same terms, listed above, as the\n"
"original.\n"
"\n"
"Updates include album art, media library search, use of the\n"
"newer SDK-2015-08-03 API's for the preferences screen, display\n"
"of the local PC IP.\n"
"\n"
"This foo_controlserver.zip component is available at : \n"
"\n"
"https://github.com/audiohead/foo_controlserver \n");
						  

