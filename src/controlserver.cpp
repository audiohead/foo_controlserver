//  $Id: controlserver.cpp 86 2006-12-04 22:29:19Z jcpolos $
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
 * Foundation, Inc., 51 Franklin Street, Ste 500, Boston, MA 02110-1301, USA.
 *
 * Nov 2016 - added album art, library search functions - Walter Hartman
 *
 */

#include "controlserver.h"
#include "chandlecallbackrun.h"

#include "debug.h"

pfc::string8 const controlserver::m_versionNumber = "1.1.4";
std::vector<SOCKET> controlserver::m_vclientSockets;
HANDLE controlserver::m_WSAendEvent = NULL;
pfc::string8 controlserver::m_metafields;
pfc::string8 controlserver::m_delimit;
pfc::string8 controlserver::m_connectionMask;
unsigned int const controlserver::m_bufferSize = 256;
t_size controlserver::m_maxClients = 4;
bool controlserver::m_muted = false;
t_size controlserver::m_currTrackIndex = 0;
t_size controlserver::m_prevTrackIndex = 0;
t_size controlserver::m_currPlaylistIndex = 0;
metadb_handle_ptr controlserver::m_prevTrackPtr;
metadb_handle_ptr controlserver::m_currTrackPtr;
t_size controlserver::m_utf8Output = false;
t_size controlserver::m_preventClose = true;
albumart controlserver::m_albumart;

// constructor
controlserver::controlserver()
{

};

// destructor
controlserver::~controlserver()
{

};


bool
controlserver::convertFromWide(pfc::string8 const& incoming, pfc::string8& output)
{
    char* cleanData = NULL;

    cleanData = (char*)calloc(incoming.length()+1,sizeof(char));

    // unable to allocate memory
    if (cleanData == NULL)
    {
        return false;
    }

    //out should be at least strlen(src)+1 long
    pfc::convert_to_lower_ascii(incoming, strlen(incoming.get_ptr()), cleanData);

    output = cleanData;

    return true;
}

// used to send data to clients
bool
controlserver::sendData(SOCKET clientSocket, pfc::string8 const& data)
{
    pfc::string8 cleanData;

    if (!m_utf8Output)
    {
        convertFromWide(data, cleanData);
    }
    else
    {
        cleanData = data;
    }

    if ((send(clientSocket, cleanData.get_ptr(), strlen(cleanData.get_ptr()), 0)) == SOCKET_ERROR)
    {
        int lastError = WSAGetLastError();
        pfc::string8 clientAddress = calculateSocketAddress(clientSocket);

        if (lastError != WSAEWOULDBLOCK)
        {
            pfc::string8 msg;

            msg << "foo_controlserver: error " << lastError << " on send() to client at " << clientAddress;
            console::info(msg);

            return false;
        } else
        {
//            msg << "foo_controlserver: error " << lastError << " on send() to client at " << clientAddress;
//            msg << " (WSAWOULDBLOCK non-critical";
//            console::info(msg);

            return true;
        }
    }

    return true;
}

// metafield in options was updated, update our copy
// should probably have one struct for this w/ mutex
void
controlserver::handleFieldsChange(pfc::string8 const& wtf)
{
    m_metafields = wtf;
}

void
controlserver::handleConnectionMaskChange(pfc::string8 const& wtf)
{
    m_connectionMask = wtf;
}

void
controlserver::handleMaxClientsChange(t_size s)
{
    m_maxClients = s;
}

// delimiter in options was updated, update our copy
void
controlserver::handleDelimitStringChange(pfc::string8 const& wtf)
{
    m_delimit = wtf;
}

void
controlserver::handleUtf8OutputChange(t_size outputType)
{
    m_utf8Output = outputType;
}

void
controlserver::handlePreventCloseChange(t_size prevent)
{
    m_preventClose = prevent;
}

void
controlserver::calculateTrackStateHeader(pfc::string8& p_trackStr, int type)
{
    static_api_ptr_t<playback_control> pbc;

    //  are we paused
    if (pbc->is_paused())
    {
        if (type == 0)
        {
            p_trackStr << "113";
        }
        else if (type == 1)
        {
            p_trackStr << "603";
        }
        else
        {
            p_trackStr << "503";
        }
    }
    else
    {
        // not paused are we playing?
        if (pbc->is_playing())
        {
            if (type == 0)
            {
                p_trackStr << "111";
            }
            else if (type == 1)
            {
                p_trackStr << "602";
            }
            else
            {
                p_trackStr << "502";
            }
        }
        else
        {
            // not playing either we must be stopped
            if (type == 0)
            {
                p_trackStr << "112";
            }
            else if (type == 1)
            {
                p_trackStr << "604";
            }
            else
            {
                p_trackStr << "504";
            }
        }
    }
}

// fill in the string with the track metadata
void
controlserver::generateSearchString(metadb_handle_ptr handle, pfc::string8& str, pfc::string8& str2)
{

    generateTrackString(handle, str, false);

    if (m_utf8Output)
    {
        t_size stringNumBytes = utf8_chars_to_bytes(str, strlen_utf8(str));
        char* tempStr = (char*)calloc(stringNumBytes+1, sizeof(char));

        if (tempStr == NULL)
        {
            return;
        }

        memcpy(tempStr, str.get_ptr(), stringNumBytes);

        _strlwr_s(tempStr, stringNumBytes+1);

        str = tempStr;

        free(tempStr);

        // now convert our search string to lower too
        stringNumBytes = utf8_chars_to_bytes(str2, strlen_utf8(str2));
        char* tempStr2 = (char*)calloc(stringNumBytes+1, sizeof(char));

        if (tempStr2 == NULL)
        {
            return;
        }

        memcpy(tempStr2, str2.get_ptr(), stringNumBytes);

        _strlwr_s(tempStr2, stringNumBytes+1);

        str2 = tempStr2;

        free(tempStr2);

        tempStr = NULL;
        tempStr2 = NULL;
    }
    else
    {
        char* tempStr = (char*)calloc(strlen(str.get_ptr())+1, sizeof(char));
        char* tempStr2 = (char*)calloc(strlen(str2.get_ptr())+1, sizeof(char));

        // convert track str to lower
        pfc::convert_to_lower_ascii(str, strlen(str.get_ptr()), tempStr);
        _strlwr_s(tempStr, strlen(str.get_ptr())+1);

        // now convert our search string to lower too
        pfc::convert_to_lower_ascii(str2, strlen(str2.get_ptr()), tempStr2);
        _strlwr_s(tempStr2, strlen(str2.get_ptr())+1);

        str = tempStr;
        str2 = tempStr2;

        free(tempStr);
        free(tempStr2);

        tempStr = NULL;
        tempStr2 = NULL;
    }
}

// fill in the string with the track metadata
void
controlserver::generateTrackString(metadb_handle_ptr handle, pfc::string8& str, bool pos)
{
    pfc::string8 field;
    static_api_ptr_t<playback_control> pbc;
    unsigned int i = 0;

    if (handle == NULL)
    {
        return;
    }

    if (pos)
    {
        str << pfc::format_float(pbc->playback_get_position(), 2, 2) << m_delimit;
    }

	service_ptr_t<titleformat_object> script;
    pfc::string8 text;

    // mutex on metafields
    static_api_ptr_t<titleformat_compiler>()->compile_safe(script, controlserver::m_metafields);

    pbc->playback_format_title_ex(handle, NULL, text, script, NULL, play_control::display_level_all);

    str << text << m_delimit << "\r\n";
}

void
controlserver::initTrackers()
{
    static_api_ptr_t<playlist_manager> plm;
    metadb_handle_ptr handlept;
    t_size indexPlayList = 0;
    t_size indexToUse = 0;

    if (plm->get_playing_item_location(&indexPlayList, &indexToUse))
    {
        plm->playlist_get_item_handle(handlept, indexPlayList, indexToUse);

        // update global curr track
        m_currTrackIndex = indexToUse;
        m_currPlaylistIndex = indexPlayList;
    }
    else
    {
        // bit of a crude way so that we always play a stopped track on start up
        // just get the focus of the last item in playlist
        m_currPlaylistIndex = plm->get_active_playlist();

        // handle
        plm->playlist_get_focus_item_handle(handlept, m_currPlaylistIndex);

        // update global current track
        m_currTrackIndex = plm->playlist_get_focus_item(m_currPlaylistIndex);
    }

    controlserver::m_currTrackPtr = handlept;
    controlserver::m_prevTrackPtr = handlept;
    m_prevTrackIndex = m_currTrackIndex;
}

pfc::string8 controlserver::generateNowPlayingTrackString()
{
	static_api_ptr_t<play_control> pc;
	metadb_handle_ptr pb_item_ptr;
	pfc::string8 str = "";

	if (pc->get_now_playing(pb_item_ptr))
	{
		generateTrackString(pb_item_ptr, str, false);
	}
	return str;
}

// mod playlist # pos if not found use -1 -1 rest correct, copilot if it sees -1 -1 knows not in
// playlist
// song updated, notify a single client
void
controlserver::handleTrackUpdateOnConnect(SOCKET clientSocket)
{
    metadb_handle_ptr handlept;
    static_api_ptr_t<playlist_manager> plm;
    pfc::string8 sendStr;           // string we are going to send

	static_api_ptr_t<play_control> pc;
	metadb_handle_ptr pb_item_ptr;

	t_size pb_item = pfc::infinite_size;
	t_size pb_playlist = pfc::infinite_size;

	if (pc->get_now_playing(pb_item_ptr))
	{
		plm->get_playing_item_location(&pb_playlist, &pb_item);  // returns playlist # and index to playing
		
		m_currPlaylistIndex = pb_playlist;
		m_currTrackIndex = pb_item;
		
	}
    //plm->playlist_get_item_handle(handlept, m_currPlaylistIndex, m_currTrackIndex);

    if (pb_item_ptr == NULL) // no track playing
    {
		// try to extract from playlist
		plm->playlist_get_item_handle(handlept, m_currPlaylistIndex, m_currTrackIndex);

		if (handlept == NULL)  // not in playlist either
		{
			sendStr << "116" << m_delimit << m_currPlaylistIndex
				<< m_delimit << m_currTrackIndex + 1
				<< m_delimit << "Track no longer exists in playlist" << m_delimit << "\r\n";
		}
		else
		{
			calculateTrackStateHeader(sendStr, 0);
			sendStr << m_delimit << m_currPlaylistIndex;
			if (m_currTrackIndex == pfc::infinite_size)
				sendStr << m_delimit << m_currTrackIndex << m_delimit;
			else
				sendStr << m_delimit << m_currTrackIndex + 1 << m_delimit;

			// fill in string with track information from playlist
			generateTrackString(handlept, sendStr, true);

		}
	}
    else
    {
        calculateTrackStateHeader(sendStr, 0);
        sendStr << m_delimit << m_currPlaylistIndex;
		if (m_currTrackIndex == pfc::infinite_size)
			sendStr << m_delimit << m_currTrackIndex << m_delimit;
		else
			sendStr << m_delimit << m_currTrackIndex + 1 << m_delimit;

        // fill in string with playing track information
        generateTrackString(pb_item_ptr, sendStr, true);
    }

    // now send the response to the single client
    if (clientSocket != NULL)
    {
        sendData(clientSocket,sendStr);
    }
    else
    {
        // update all the clients
        for (unsigned int j=0; j<m_vclientSockets.size(); j++)
        {
            if (m_vclientSockets.at(j) != NULL)
            {
                // now send the response
                sendData(m_vclientSockets.at(j), sendStr);
            }
        }
    }
}

void
controlserver::handleTrackUpdateFromPtr(metadb_handle_ptr p_track)
{
    pfc::string8 sendStr;
    t_size indexToUse = 0;
    metadb_handle_ptr handlept;
    static_api_ptr_t<playlist_manager> plm;
    t_size playlistIndex = 0;
    t_size trackIndex = 0;
	
    if (plm->get_playing_item_location(&playlistIndex, &trackIndex))
    {
        m_currPlaylistIndex = playlistIndex;
        indexToUse = trackIndex;
    }
    else
    {
        return;
    }

    m_prevTrackPtr = m_currTrackPtr;
    m_currTrackPtr = p_track;
    m_prevTrackIndex = m_currTrackIndex;
    m_currTrackIndex = indexToUse;

    calculateTrackStateHeader(sendStr, 0);

    sendStr << m_delimit << m_currPlaylistIndex;
    sendStr << m_delimit << indexToUse+1 << m_delimit;

    // fill in string with track information
    generateTrackString(p_track, sendStr, true);

    // update all the clients
    for (unsigned int j=0; j<m_vclientSockets.size(); j++)
    {
        if (m_vclientSockets.at(j) != NULL)
        {
            // now send the response
            sendData(m_vclientSockets.at(j), sendStr);
        }
    }
}

bool
controlserver::convertStrToI(char const* str, t_size & ival)
{
    char *ep;
    long lval;

    errno = 0;
    lval = strtol(str, &ep, 10);

    // not a number
    if (str[0] == '\0' || *ep != '\0')
    {
        return false;
    }

    // out of range
    if ((errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN)) || (lval > INT_MAX || lval < INT_MIN))
    {
        return false;
    }

    ival = lval;
    return true;
}

bool
controlserver::convertStrToF(char const* str, double &ival)
{
    char *ep = '\0';

    errno = 0;
    ival = strtod(str, &ep);

    // not a number
    if (str[0] == '\0' || *ep != '\0')
    {
        return false;
    }

    // out of range
    if ((errno == ERANGE && (ival == HUGE_VAL || ival == -HUGE_VAL)))
    {
        return false;
    }

    return true;
}

void
controlserver::handleQueueTrackCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    t_size argCount = 0;
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    static_api_ptr_t<playlist_manager> plm;
    pfc::string8 msg;
    bool del = false;
    bool delAll = false;
    t_size index1 = 0;
    t_size index2 = 0;
    t_size playlistIndex = 0;
    t_size trackIndex = 0;
    t_size queueSize = 0;
    pfc::list_t<t_playback_queue_item> queueList;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "QUEUE " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 5)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a queue command";
		console::info(msg);
	}
    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (strncmp(f, "del", 3)==0 && strlen(f)==3)
                    {
                        del = true;
                    }
                    else if (!convertStrToI(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            case 1:
                {
                    if (strncmp(f, "all", 3)==0 && strlen(f) == 3)
                    {
                        delAll = true;
                    }
                    else if (!convertStrToI(f, index2))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index two conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount == 1)
    {
        if (del)
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Queue index out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }
        playlistIndex = m_currPlaylistIndex;
        trackIndex = index1;
    }
    else if (argCount == 2)
    {
        if (del)
        {
            if (delAll)
            {
                plm->queue_flush();
            }
            else
            {
                if (index2-1>=0 && index2-1<plm->queue_get_count())
                {
                    bit_array_one r(index2-1);
                    plm->queue_remove_mask(r);
                }
                else
                {
                    pfc::string8 msg;

                    msg << "999" << m_delimit << "Queue index out of range" << m_delimit << "\r\n";
                    sendData(clientSocket, msg);

                    return;
                }
            }
        }
        else
        {
            trackIndex = index2;
            playlistIndex = index1;
        }
    }
    else if (argCount > 2)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    if (argCount != 0 && !del)
    {
        if (!(playlistIndex>=0 && playlistIndex<plm->get_playlist_count()))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist index out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        if (!(trackIndex>=1 && trackIndex<=plm->playlist_get_item_count(playlistIndex)))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist track index out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        // all validation done; queue it!
        plm->queue_add_item_playlist(playlistIndex, trackIndex-1);
    }

    msg = "";
    plm->queue_get_contents(queueList);
    queueSize = plm->queue_get_count();

    // display a line with queue count
    msg << "800" << m_delimit << queueSize << m_delimit << "\r\n";

    // now display the queue
    for (t_size x=0; x<plm->queue_get_count(); x++)
    {
        pfc::string8 tmp;

        generateTrackString(queueList[x].m_handle, tmp, false);

        msg << "801" << m_delimit << x+1 << m_delimit
            << queueList[x].m_playlist << m_delimit << queueList[x].m_item+1
            << m_delimit << tmp;
    }

    sendData(clientSocket, msg);
}

void controlserver::handleLibSearchCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
	// Command form : libsearch 'playlist name' 'string'
	// strings are quoted since can have spaces

	// Build libsearch response msg 
	// "500|playlist number with results|matched count"

	// After receiving this response msg, you will need
	// to refresh your list of playlists and redownload
	// the 'library query' playlist

	pfc::string8 msg;
	pfc::string8 playlistName;
	pfc::string8 searchFor;
	char delim[] = "'";
	t_size argCount = 0;

	t_size playlist;
	t_size playlist_item_count;

	static_api_ptr_t<autoplaylist_manager> apm;
	static_api_ptr_t<playlist_manager> plm;

	pfc::string8 clientAddress = calculateSocketAddress(clientSocket);

	// strip off end of lines
	recvCommand.truncate_eol();

	int len = recvCommand.get_length();
	if (len < 11)
	{
		// no playlist name or search string
		msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
		sendData(clientSocket, msg);
		return;
	}

	char* f = strtok((char*)recvCommand.get_ptr(), delim);

	while (f != NULL)
	{
		f = strtok(NULL, delim);

		if (f != NULL)
		{
			switch (argCount)
			{
			case 0:
				// extract playlist name to be created or overwritten
				playlistName = f;
				break;

			case 1: // skip space between
				break;

			case 2:
				// extract search string
				searchFor = f;
				break;

			default:
				break;

			}
		}

		argCount++;
	}

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued libsearch " << "'" << playlistName << "'   " << "'" << searchFor << "'";
		console::info(msg);
	}

	playlist = plm->find_or_create_playlist(playlistName.toString(), pfc::infinite_size);

	if (playlist != pfc::infinite_size)
	{
		plm->set_active_playlist(playlist);

		if (apm->is_client_present(playlist))
			apm->remove_client(playlist);

		if (searchFor.get_length())
		{
			plm->playlist_undo_backup(playlist);

			try
			{
				apm->add_client_simple(searchFor.toString(), "%artist%|%album%|%discnumber%|%tracknumber%|%%title%", playlist, true);
			}
			catch (pfc::exception &e)
			{
				console::error(e.what());
			}

			if (apm->is_client_present(playlist))
				apm->remove_client(playlist);
		}

		if (playlist != pfc::infinite_size)
			playlist_item_count = plm->playlist_get_item_count(playlist);
		else
			playlist_item_count = pfc::infinite_size;

		plm->set_active_playlist(playlist);
	
		pfc::string8 msg;
		if (playlist != pfc::infinite_size)
		{
			// "500|playlist number with results|matched count"
			msg << "500" << m_delimit << playlist << m_delimit
				<< playlist_item_count << m_delimit << "\r\n";
		}
		if (CONSOLE_DEBUG) console::info(msg);
		sendData(clientSocket, msg);

	}

}

pfc::string8 trim(pfc::string8 &str)
{
	pfc::string8 out = str;

	size_t i = 0;

	while (i < out.length() && out[i] == ' ')
		++i;

	if (i == out.length())
		return pfc::string8("");

	out.remove_chars(0, i);

	i = out.length() - 1;

	while (i > 0 && out[i] == ' ')
		--i;

	if (i < out.length())
		out.truncate(i + 1);

	return out;
}

bool controlserver::writeImageFile(char *filename, unsigned char *buf, int size)
{
	FILE *fp = fopen(filename, "wb");
	if (fp != NULL)
	{
		int outsize = fwrite((void *)buf, sizeof(unsigned char), size, fp);
		fclose(fp);
		if (outsize == size)
			return true;
		else
			return false;
	}
	else
	{
		return false;
	}
}

bool controlserver::readImageFile(char *img, unsigned char *buf)
{
	FILE *file = fopen(img, "rb");
	if (file != NULL)
	{
		struct stat finfo;
		stat(img, &finfo);
		int fileSize = finfo.st_size;

		//int fileLen;
		//fseek(file, 0, SEEK_END);
		//fileLen = ftell(file);
		//fseek(file, 0, SEEK_SET);

		buf = (unsigned char *)malloc(fileSize +1);
		fread(buf, fileSize, 1, file);
		unsigned char x;
		for (int i = 0; i < fileSize; i++)
		{
			x = *buf++;
		}
		fclose(file);
		// convert to base64
	}
	return true;
}

void controlserver::retrieveAlbumArt(albumart &art_out)
{
	static_api_ptr_t<playback_control_v2> pc;
	metadb_handle_ptr pb_track_ptr;

	art_out.reset();

	pc->get_now_playing(pb_track_ptr);
	if (pb_track_ptr == NULL)
	{
		art_out.pb_albumart_status = albumart::AS_NO_INFO;
		if (CONSOLE_DEBUG)
		{
			pfc::string8 msg;
			msg << "pb ptr null so NO_INFO album art";
			console::info(msg);
		}

		return;
	}

	art_out.pb_albumart_status = albumart::AS_NOT_FOUND;

	// extract track title for now playing track
	pfc::string8 title;
	service_ptr_t<titleformat_object> script;
	static_api_ptr_t<titleformat_compiler>()->compile_safe(script, "%title%");

	pb_track_ptr->format_title(NULL, title, script, NULL);
	art_out.trackTitle = title;

	// extract album title for now playing track
	pfc::string8 album;
	service_ptr_t<titleformat_object> script2;
	static_api_ptr_t<titleformat_compiler>()->compile_safe(script2, "%album%");

	pb_track_ptr->format_title(NULL, album, script2, NULL);
	art_out.albumTitle = album;

	pfc::list_t<GUID> guids;
	guids.add_item(album_art_ids::cover_front);
	metadb_handle_list tracks;
	tracks.add_item(pb_track_ptr);

	// Foobar2000 do album art search using settings defined under :
	// Files>Preferences>Advanced>Display>Album Art
	// So either a picture file or embedded album art. 
	// If an external file, it will look for jpg or png files named "folder", "cover", "front", 
	// "artist name".

	static_api_ptr_t<album_art_manager_v3> aam;
	abort_callback_dummy abortCallback;
	album_art_data::ptr art_ptr;

	bool found = false;
	album_art_extractor_instance_v2::ptr extractor = aam->open_v3(tracks, guids, NULL, abortCallback);

	found = extractor->query(album_art_ids::cover_front, art_ptr, abortCallback);
	if (!found)
	{
		found = extractor->query(album_art_ids::cover_back, art_ptr, abortCallback);
		if (!found)
		{
			found = extractor->query(album_art_ids::disc, art_ptr, abortCallback);
			if (!found)
			{
				found = extractor->query(album_art_ids::artist, art_ptr, abortCallback);
			}
		}
	}

	if (found)
	{
		art_out.pb_albumart_status = albumart::AS_FOUND;

		if (art_ptr.is_valid())
		{
			art_out.size = art_ptr->get_size();
			if (CONSOLE_DEBUG)
			{
				pfc::string8 msg;
				msg << "found embedded art : size = " << art_out.size;
				msg << " title=" << art_out.trackTitle;
				msg << " album=" << art_out.albumTitle;
				console::info(msg);
			}

			// Buffer contains contents of image file
			unsigned char *buffer = new unsigned char[art_out.size];
			memcpy((void *)buffer, (void *)art_ptr->get_ptr(), art_out.size);

			// Convert image to base64 for transfer to client
			base64_encode(art_out.base64string, (const void *)buffer, art_out.size);

		}
	}
	else
	{
		if (CONSOLE_DEBUG)
		{
			pfc::string8 msg;
			msg << "no album art found ";
			msg << " title=" << art_out.trackTitle;
			msg << " album=" << art_out.albumTitle;
			console::info(msg);
		}

	}
}

// handle next command
void
controlserver::handleNextCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    t_size argCount = 0;
    t_size playlistIndex = 0;
    t_size index1 = 0;
    static_api_ptr_t<playlist_manager> plm;
    static_api_ptr_t<play_control> pbc;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "NEXT " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 4)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a next command";
		console::info(msg);
	}

    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (!convertStrToI(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount == 1)
    {
        playlistIndex = index1;
    }
    else if (argCount > 1)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    if (argCount != 0)
    {
        if (!(playlistIndex>=0 && playlistIndex<plm->get_playlist_count()))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist index out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        // all validation done; set playlist it!
        plm->set_playing_playlist(playlistIndex);
    }

    pbc->start(play_control::track_command_next);
}

// handle prev command
void
controlserver::handlePrevCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    t_size argCount = 0;
    t_size playlistIndex = 0;
    t_size index1 = 0;
    static_api_ptr_t<playlist_manager> plm;
    static_api_ptr_t<play_control> pbc;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "PREV " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 4)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a prev command";
		console::info(msg);
	}

    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (!convertStrToI(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount == 1)
    {
        playlistIndex = index1;
    }
    else if (argCount > 1)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    if (argCount != 0)
    {
        if (!(playlistIndex>=0 && playlistIndex<plm->get_playlist_count()))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist index out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        // all validation done; set playlist it!
        plm->set_playing_playlist(playlistIndex);
    }

    pbc->start(play_control::track_command_prev);
}

// handle random command
void
controlserver::handleRandomCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    t_size argCount = 0;
    t_size playlistIndex = 0;
    t_size index1 = 0;
    static_api_ptr_t<playlist_manager> plm;
    static_api_ptr_t<play_control> pbc;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "rand " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 4)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a random command";
		console::info(msg);
	}

    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (!convertStrToI(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount == 1)
    {
        playlistIndex = index1;
    }
    else if (argCount > 1)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    if (argCount != 0)
    {
        if (!(playlistIndex>=0 && playlistIndex<plm->get_playlist_count()))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist index out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        // all validation done; set playlist it!
        plm->set_playing_playlist(playlistIndex);
    }

    pbc->start(play_control::track_command_rand);
}

// handle stop command
void
controlserver::handleStopCommand(SOCKET clientSocket)
{
    static_api_ptr_t<play_control> pbc;
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a stop command";
		console::info(msg);
	}
    pbc->stop();
}

// handle pause command
void
controlserver::handlePauseCommand(SOCKET clientSocket)
{
    static_api_ptr_t<play_control> pbc;
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a pause command";
		console::info(msg);
	}
    pbc->toggle_pause();
}

// handle volume change
void
controlserver::handleVolumeChange(float new_val)
{
    pfc::string8 sendStr;

    sendStr << "222" << m_delimit << pfc::format_float(new_val, 2, 2) << m_delimit << "\r\n";

    for (unsigned int i=0; i<m_vclientSockets.size(); i++)
    {
        if (m_vclientSockets.at(i) != NULL)
        {
            // now send the response
            sendData(m_vclientSockets.at(i), sendStr);
        }
    }
}

// handle volume set command
void
controlserver::handleVolumeSetCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    t_size argCount = 0;
    double vol = 0;
    double index1 = 0;
    bool volmute = false;
    bool volup = false;
    bool voldown = false;
    static_api_ptr_t<playlist_manager> plm;
    static_api_ptr_t<play_control> pbc;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "vol " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 3)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		//msg << "foo_controlserver: client from " << clientAddress << " issued a volume command";
		//console::info(msg);
	}
    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (strncmp(f, "up", 2)==0 && strlen(f)==2)
                    {
                        volup = true;
                    }
                    else if (strncmp(f, "down", 4)==0 && strlen(f)==4)
                    {
                        voldown = true;
                    }
                    else if (strncmp(f, "mute", 4)==0 && strlen(f)==4)
                    {
                        volmute = true;
                    }
                    else if (!convertStrToF(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount == 1)
    {
        if (volup)
        {
            if (m_muted)
            {
                pbc->volume_mute_toggle();
                m_muted = false;
            }
            else
            {
                pbc->volume_up();
            }
            return;
        }
        else if (voldown)
        {
            pbc->volume_down();
            return;
        }
        else if (volmute)
        {
            pbc->volume_mute_toggle();

            m_muted = !m_muted;

            return;
        }
        else
        {
            vol = index1;
        }
    }
    else if (argCount > 1)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }
    else
    {
        pfc::string8 sendStr;

        sendStr << "222" << m_delimit << pfc::format_float(pbc->get_volume(), 2, 2) << m_delimit << "\r\n";

        sendData(clientSocket, sendStr);

        return;
    }

    pbc->set_volume((float)vol);
}

// handle play list command
void
controlserver::handleListCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    pfc::string8 sendStr;
    t_size argCount = 0;
    t_size index2 = 0;
    t_size index3 = 0;
    t_size index1 = 0;
    t_size playlistIndex = 0;
    t_size trackIndex1 = 0;
    t_size trackIndex2 = 0;
    static_api_ptr_t<playlist_manager> plm;
    static_api_ptr_t<play_control> pbc;
    metadb_handle_list list;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "list " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 4)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a list command";
		console::info(msg);
	}

    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (!convertStrToI(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            case 1:
                {
                    if (!convertStrToI(f, index2))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index two conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            case 2:
                {
                    if (!convertStrToI(f, index3))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index three conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount == 0)
    {
        playlistIndex = m_currPlaylistIndex;
        trackIndex1 = 1;
        trackIndex2 = plm->playlist_get_item_count(playlistIndex);
    }
    else if (argCount == 1)
    {
        playlistIndex = index1;
        trackIndex1 = 1;
        trackIndex2 = plm->playlist_get_item_count(playlistIndex);
    }
    else if (argCount == 2)
    {
        playlistIndex = m_currPlaylistIndex;
        trackIndex1 = index1;
        trackIndex2 = index2;
    }
    else if (argCount == 3)
    {
        playlistIndex = index1;
        trackIndex1 = index2;
        trackIndex2 = index3;
    }
    else if (argCount > 3)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    // validate indexes against playlists
    if (!(playlistIndex>=0 && playlistIndex<plm->get_playlist_count()))
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Playlist index out of range" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    if (!(trackIndex1>=1 && trackIndex1-1<plm->playlist_get_item_count(playlistIndex)))
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Playlist track index one out of range" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    if (!(trackIndex2>=1 && trackIndex2-1<plm->playlist_get_item_count(playlistIndex)))
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Playlist track index two out of range" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    if (trackIndex1 > trackIndex2)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Playlist track index one greater than playlist track index two" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    bit_array_range r(trackIndex1-1, trackIndex2-trackIndex1+1);
    plm->playlist_get_items(playlistIndex, list, r);

    // nothing to list, playlist is empty
    if (list.get_count() == 0)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << playlistIndex << m_delimit
            << "Play list contains zero entries" << m_delimit << "\r\n";

        sendData(clientSocket, msg);

        return;
    }

    sendStr << "600" << m_delimit << playlistIndex << m_delimit
            << list.get_count() << m_delimit << "\r\n";

    for (t_size i=0; i<list.get_count(); i++)
    {
        if (trackIndex1-1 == m_currTrackIndex && playlistIndex == m_currPlaylistIndex)
        {
            calculateTrackStateHeader(sendStr, 1);
        }
        else
        {
            sendStr << "601";  
        }

        sendStr << m_delimit << playlistIndex << m_delimit
                << trackIndex1 << m_delimit;

        generateTrackString(list[i],sendStr, false);

        trackIndex1++;
    }

    // finally send our big result
    // could break this into chunks
    sendData(clientSocket, sendStr);
}

// handle play command
void
controlserver::handlePlayCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    t_size argCount = 0;
    t_size index2 = 0;
    t_size index1 = 0;
    t_size playlistIndex = 0;
    t_size trackIndex = 0;
    static_api_ptr_t<playlist_manager> plm;
    static_api_ptr_t<play_control> pbc;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "play " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    // make sure it is just "play"
    if (strlen(f) != 4)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a play command";
		console::info(msg);
	}

    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (!convertStrToI(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            case 1:
                {
                    if (!convertStrToI(f, index2))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index two conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount == 1)
    {
        playlistIndex = m_currPlaylistIndex;
        trackIndex = index1;
    }
    else if (argCount == 2)
    {
        trackIndex = index2;
        playlistIndex = index1;
    }
    else if (argCount > 2)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    if (argCount != 0)
    {
        if (!(playlistIndex>=0 && playlistIndex<plm->get_playlist_count()))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist index out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        if (!(trackIndex>=1 && trackIndex<=plm->playlist_get_item_count(playlistIndex)))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist track index out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        // all validation done; set focus for playing it!
        plm->set_playing_playlist(playlistIndex);
        plm->set_active_playlist(playlistIndex);
        plm->playlist_set_focus_item(playlistIndex, trackIndex-1);
    }

    pbc->start(playback_control::track_command_settrack);
}

// handle a help command, this will be in the client thread
// not called from the main foobar application thread (though it could:p)
void
controlserver::handleHelpCommand(SOCKET clientSocket, bool verbose)
{
    pfc::string8 msg;
    pfc::string8 sendStr;
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a help command";
		console::info(msg);
	}

    if (verbose)
    {
        sendStr <<
        "999" << m_delimit << "next [playlist#]                                       - play next track in the playlist" << m_delimit << "\r\n" <<
        "999" << m_delimit << "prev [playlist#]                                       - play previous track in the playlist" << m_delimit << "\r\n" <<
        "999" << m_delimit << "play [track#]|[<playlist#> <track#>]                   - play current or item # from the playlist" << m_delimit << "\r\n" <<
        "999" << m_delimit << "rand [playlist#]                                       - play a random track from the playlist" << m_delimit << "\r\n" <<
        "999" << m_delimit << "seek ['delta'] <secs>                                  - seek to specified time or by delta time" << m_delimit << "\r\n" <<
        "999" << m_delimit << "pause                                                  - pause/unpause current track" << m_delimit << "\r\n" <<
        "999" << m_delimit << "stop                                                   - stop current track" << m_delimit << "\r\n" <<
        "999" << m_delimit << "list [playlist#] [<track#> <track#>]                   - list playlist tracks between range" << m_delimit << "\r\n" <<
        "999" << m_delimit << "libsearch 'playlist name' 'string'                     - media search for info, load results into playlist 'playlist name', use quotes on strings" << m_delimit << "\r\n" <<
        "999" << m_delimit << "listinfo [playlist#]|[<playlist#> <playlist#>]|['all'] - info about current playlist or all lists" << m_delimit << "\r\n" <<
        "999" << m_delimit << "serverinfo                                             - info about the server" << m_delimit << "\r\n" <<
        "999" << m_delimit << "trackinfo                                              - info about the current track" << m_delimit << "\r\n" <<
        "999" << m_delimit << "vol [#]|['up']|['down']|['mute']                       - get/set volume dB or up/down 0.50 dB or mute" << m_delimit << "\r\n" <<
        "999" << m_delimit << "order [type]                                           - get/set order type [default|random|repeatplaylist|repeattrack|shuffletrack|shufflealbum|shufflefolder]" << m_delimit << "\r\n" <<
        "999" << m_delimit << "queue [[playlist#] <track#>]                           - queue track from playlist or view queue" << m_delimit << "\r\n" <<
        "999" << m_delimit << "queue del <index#>|<'all'>                             - delete queue index item from queue or clear all queue" << m_delimit << "\r\n" <<
		"999" << m_delimit << "albumart                                               - get album art for now playing track" << m_delimit << "\r\n";

        if (!m_preventClose)
        {
            sendStr << "999" << m_delimit << "fooexit                                                - close foobar2000 (careful!)" << m_delimit << "\r\n";
        }

        sendStr << "999" << m_delimit << "exit                                                   - disconnect from server" << m_delimit << "\r\n";
    }
    else
    {
        sendStr <<
        "999" << m_delimit << "next [playlist#]" << m_delimit << "\r\n" <<
        "999" << m_delimit << "prev [playlist#]" << m_delimit << "\r\n" <<
        "999" << m_delimit << "play [track#]|[<playlist#> <track#>]" << m_delimit << "\r\n" <<
        "999" << m_delimit << "rand [playlist#]" << m_delimit << "\r\n" <<
        "999" << m_delimit << "seek ['delta'] <secs>" << m_delimit << "\r\n" <<
        "999" << m_delimit << "pause" << m_delimit << "\r\n" <<
        "999" << m_delimit << "stop" << m_delimit << "\r\n" <<
        "999" << m_delimit << "list [playlist#] [<track#> <track#>]" << m_delimit << "\r\n" <<
		"999" << m_delimit << "libsearch 'playlist name' 'string'" << m_delimit << "\r\n" <<
        "999" << m_delimit << "listinfo [playlist#]|[<playlist#> <playlist#>]|['all']" << m_delimit << "\r\n" <<
        "999" << m_delimit << "serverinfo" << m_delimit << "\r\n" <<
        "999" << m_delimit << "trackinfo" << m_delimit << "\r\n" <<
        "999" << m_delimit << "vol [#]|['up']|['down']|['mute']" << m_delimit << "\r\n" <<
        "999" << m_delimit << "order ['default'|'random'|'repeatplaylist'|'repeattrack'|'shuffletrack'|'shufflealbum'|'shufflefolder']" << m_delimit << "\r\n" <<
        "999" << m_delimit << "queue [[playlist#] <track#>]" << m_delimit << "\r\n" <<
        "999" << m_delimit << "queue del <index#>|<'all'>" << m_delimit << "\r\n" <<
		"999" << m_delimit << "albumart" << m_delimit << "\r\n";

        if (!m_preventClose)
        {
            sendStr << "999" << m_delimit << "fooexit" << m_delimit << "\r\n";
        }

        sendStr << "999" << m_delimit << "exit" << m_delimit << "\r\n";
    }

    // now send the response
    sendData(clientSocket, sendStr);
}

// not called from the main foobar application thread (though it could:p)
void
controlserver::handleServerInfoCommand(SOCKET clientSocket)
{
    pfc::string8 msg;
    pfc::string8 sendStr;
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a serverinfo command";
		console::info(msg);
	}
    sendStr << "999" << m_delimit << "Connected to foobar2000 Control Server " << m_versionNumber << m_delimit << "\r\n" <<
               "999" << m_delimit << "You are a client from " << calculateSocketAddress(clientSocket) << m_delimit << "\r\n" <<
               "999" << m_delimit << "There are currently " << m_vclientSockets.size() << "/" << m_maxClients << " clients connected" << m_delimit << "\r\n";

    for (unsigned int x=0; x<m_vclientSockets.size(); x++)
    {
        sendStr << "999" << m_delimit << "client " << x+1 << ": ";

        if (m_vclientSockets.at(x) != NULL)
        {
            sendStr << calculateSocketAddress(m_vclientSockets.at(x)) << m_delimit << "\r\n";
        }
        else
        {
            sendStr << "not connected" << m_delimit << "\r\n";
        }
    }

    // now send the response
    sendData(clientSocket, sendStr);
}

void
controlserver::handleOrderCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    t_size argCount = 0;
    static_api_ptr_t<playlist_manager> plm;
    pfc::string8 arg1;
    pfc::string8 strType;
    pfc::string8 sendStr;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "order " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 5)
    {
        return;
    }

    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    arg1 = f;
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount > 1)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }
    else if (argCount == 0)
    {
        pfc::string8 msg;
        t_size orderIndex = plm->playback_order_get_active();

		if (CONSOLE_DEBUG)
		{
			msg << "foo_controlserver: client from " << clientAddress << " issued an order request command";
			console::info(msg);
		}
        strType = plm->playback_order_get_name(orderIndex);
    }
    else if (argCount == 1)
    {
        pfc::string8 msg;

        if (strncmp(arg1,"default",7) == 0)
        {
			if (CONSOLE_DEBUG)
			{
				msg << "foo_controlserver: client from " << clientAddress << " issued an order command 'default'";
				console::info(msg);
			}
            strType = plm->playback_order_get_name(0);
            plm->playback_order_set_active(0);
        }
        else if (strncmp(arg1,"repeatplaylist",14) == 0)
        {
			if (CONSOLE_DEBUG)
			{
				msg << "foo_controlserver: client from " << clientAddress << " issued an order command 'repeatplaylist'";
				console::info(msg);
			}
            strType = plm->playback_order_get_name(1);
            plm->playback_order_set_active(1);
        }
        else if (strncmp(arg1,"repeattrack",11) == 0)
        {
			if (CONSOLE_DEBUG)
			{
				msg << "foo_controlserver: client from " << clientAddress << " issued an order command 'repeattrack'";
				console::info(msg);
			}
            strType = plm->playback_order_get_name(2);
            plm->playback_order_set_active(2);
        }
		else if (strncmp(arg1,"random",14) == 0)
        {
			if (CONSOLE_DEBUG)
			{
				msg << "foo_controlserver: client from " << clientAddress << " issued an order command 'repeatplaylist'";
				console::info(msg);
			}
            strType = plm->playback_order_get_name(3);
            plm->playback_order_set_active(3);
        }
        else if (strncmp(arg1,"shuffletrack",12) == 0)
        {
			if (CONSOLE_DEBUG)
			{
				msg << "foo_controlserver: client from " << clientAddress << " issued an order command 'shuffletrack'";
				console::info(msg);
			}

            strType = plm->playback_order_get_name(4);
            plm->playback_order_set_active(4);
        }
        else if (strncmp(arg1,"shufflealbum",12) == 0)
        {
			if (CONSOLE_DEBUG)
			{
				msg << "foo_controlserver: client from " << clientAddress << " issued an order command 'shufflealbum'";
				console::info(msg);
			}
            strType = plm->playback_order_get_name(5);
            plm->playback_order_set_active(5);
        }
        else if (strncmp(arg1,"shufflefolder",16) == 0)
        {
			if (CONSOLE_DEBUG)
			{
				msg << "foo_controlserver: client from " << clientAddress << " issued an order command 'shuffledirectory'";
				console::info(msg);
			}
            strType = plm->playback_order_get_name(6);
            plm->playback_order_set_active(6);
        }
        else
        {
            pfc::string8 msg;

            // was not a good order command
            msg << "999" << m_delimit << "Invalid order type" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }
    }

    sendStr << "333" << m_delimit << strType << m_delimit << "\r\n";

    // update all the clients
    for (unsigned int j=0; j<m_vclientSockets.size(); j++)
    {
        if (m_vclientSockets.at(j) != NULL)
        {
            // now send the response
            sendData(m_vclientSockets.at(j),sendStr);
        }
    }
}


void
controlserver::handleAlbumArtCommand(SOCKET clientSocket)
{
	pfc::string8 msg;
	pfc::string8 sendStr;
	pfc::string8 clientAddress = calculateSocketAddress(clientSocket);

	// fetches album art for 'now playing' track from foobar2000.
	// Embeds jpg, png, etc file into return datastream
	// as a base64 text string.
	// Response is sent back as a 2 part message, see below for details

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued an albumart command";
		console::info(msg);
	}

	retrieveAlbumArt(m_albumart);

	int base64Length = m_albumart.base64string.get_length();
	if (m_albumart.pb_albumart_status == albumart::AS_NOT_FOUND ||
		m_albumart.pb_albumart_status == albumart::AS_NO_INFO)
	{
		// No album art found
		m_albumart.numBlocks = -1;
		m_albumart.size = 0;
	}
	else
	{
		// If there is album art, send it in one message block as base.
		// Image content is the raw bits of the jpg or png file
		m_albumart.numBlocks = 1;
	}

	// Send 2 part album art response
	// 700|numBlocks|base64Length|size of album art image file|trackTitle|albumTitle|
	// 701|numBlocks|base64string of image|
	// numBlocks = -1 if no image file, else always 1 (one block)
	// image file is transferred as a base64 string
	sendStr << "700" << m_delimit << m_albumart.numBlocks << m_delimit << base64Length << m_delimit << m_albumart.size << m_delimit << m_albumart.trackTitle << m_delimit << m_albumart.albumTitle << m_delimit << "\r\n";
    sendStr << "701" << m_delimit << m_albumart.numBlocks << m_delimit << m_albumart.base64string << m_delimit << "\r\n";

	// send the response
	sendData(clientSocket, sendStr);
}



// handle a list info command
void
controlserver::handleListInfoCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    pfc::string8 sendStr;
    pfc::string8 listName;
    t_size argCount = 0;
    unsigned int index1 = 0;
    unsigned int index2 = 0;
    bool all = false;
    static_api_ptr_t<playlist_manager> plm;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "listinfo " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 8)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a listinfo command";
		console::info(msg);
	}

    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (strncmp(f, "all", 3)==0 && strlen(f)==3)
                    {
                        all = true;
                    }
                    else if (!convertStrToI(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
        
            case 1:
                {
                    if (!convertStrToI(f, index2))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index two conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }

                }
                break;
                
            }
            
            argCount++;
        }
    }

    if (argCount == 1 || argCount == 0)
    {
        if (all)
        {
            t_size z = 0;

            sendStr << "400" << m_delimit << plm->get_playlist_count() << m_delimit << "\r\n";

            for(z=0; z<plm->get_playlist_count(); z++)
            {
                plm->playlist_get_name(z, listName);

                if (z == m_currPlaylistIndex)
                {
                    sendStr << "402";
                }
                else
                {
                    sendStr << "401";
                }

                sendStr << m_delimit << z << m_delimit << listName << m_delimit
                        << plm->playlist_get_item_count(z) << m_delimit << "\r\n";
            }
        }
        else
        {
            if (argCount == 0)
            {
                index1 = m_currPlaylistIndex;
            }

            if (!(index1>=0 && index1<plm->get_playlist_count()))
            {
                pfc::string8 msg;

                msg << "999" << m_delimit << "Playlist index out of range" << m_delimit << "\r\n";
                sendData(clientSocket, msg);

                return;
            }

            plm->playlist_get_name(index1, listName);

            if (index1 == m_currPlaylistIndex)
            {
                sendStr << "402";
            }
            else
            {
                sendStr << "404";
            }

            sendStr << m_delimit << index1 << m_delimit << listName << m_delimit
                    << plm->playlist_get_item_count(index1) << m_delimit << "\r\n";
        }
    }
    else if (argCount == 2)
    {
        t_size z = 0;

        if (!(index1>=0 && index1<plm->get_playlist_count()))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist index one out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        if (!(index2>=0 && index2<plm->get_playlist_count()))
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist index two out of range" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        if (index1 > index2)
        {
            pfc::string8 msg;

            msg << "999" << m_delimit << "Playlist index one greater than playlist index two" << m_delimit << "\r\n";
            sendData(clientSocket, msg);

            return;
        }

        sendStr << "400" << m_delimit << (index2-index1)+1 << m_delimit << "\r\n";

        for(z=index1; z<=index2; z++)
        {
            plm->playlist_get_name(z, listName);

            if (z == m_currPlaylistIndex)
            {
                sendStr << "402";
            }
            else
            {
                sendStr << "401";
            }

            sendStr << m_delimit << z << m_delimit << listName << m_delimit
                    << plm->playlist_get_item_count(z) << m_delimit << "\r\n";
        }
    }
    else if (argCount > 2)
    {
        pfc::string8 msg;

        msg << "999" << m_delimit << "Invalid number of arguments" << m_delimit << "\r\n";
        sendData(clientSocket, msg);

        return;
    }

    // send response
    sendData(clientSocket, sendStr);
}

void
controlserver::handleSeekTrackCommand(SOCKET clientSocket, pfc::string8 recvCommand)
{
    char delim[] = " ";
    t_size argCount = 0;
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    static_api_ptr_t<playlist_manager> plm;
    static_api_ptr_t<playback_control> pbc;
    pfc::string8 msg;
    double index1 = 0;
    double index2 = 0;
    bool delta = false;

    // strip off end of lines
    recvCommand.truncate_eol();

    // strip off the "seek " part
    char* f = strtok((char*)recvCommand.get_ptr(), delim);

    if (f == NULL)
    {
        return;
    }

    if (strlen(f) != 4)
    {
        return;
    }

	if (CONSOLE_DEBUG)
	{
		msg << "foo_controlserver: client from " << clientAddress << " issued a seek command";
		console::info(msg);
	}

    while (f != NULL)
    {
        f = strtok(NULL, delim);

        if (f != NULL)
        {
            switch(argCount)
            {
            case 0:
                {
                    if (strncmp(f, "delta", 5)==0 && strlen(f)==5)
                    {
                        delta = true;
                    }
                    else if (!convertStrToF(f, index1))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index one conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            case 1:
                {
                    if (!convertStrToF(f, index2))
                    {
                        pfc::string8 msg;

                        msg << "999" << m_delimit << "Invalid index two conversion" << m_delimit << "\r\n";
                        sendData(clientSocket, msg);

                        return;
                    }
                }
                break;
            }

            argCount++;
        }
    }

    if (argCount == 1)
    {
        pbc->playback_seek(index1);
    }
    else if (argCount == 2 && delta)
    {
        pbc->playback_seek_delta(index2);
    }
}


// handle a track info command
void
controlserver::handleTrackInfoCommand(SOCKET clientSocket)
{
	metadb_handle_ptr pb_item_ptr = NULL;
    metadb_handle_ptr handlept = NULL;
    pfc::string8 sendStr;
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    pfc::string8 msg;
    static_api_ptr_t<playlist_manager> plm;
	static_api_ptr_t<play_control> pc;

	if (CONSOLE_DEBUG)
	{
		//msg << "foo_controlserver: client from " << clientAddress << " issued a trackinfo command";
		//console::info(msg);
	}
	t_size pb_item = pfc::infinite_size;
	t_size pb_playlist = pfc::infinite_size;

	if (pc->get_now_playing(pb_item_ptr))
	{
		plm->get_playing_item_location(&pb_playlist, &pb_item);  // returns playlist # and index to playing
		
		m_currPlaylistIndex = pb_playlist;
		m_currTrackIndex = pb_item;
		
	}

	if (pb_item_ptr == NULL) // no track playing
	{
		// try to extract from playlist
		plm->playlist_get_item_handle(handlept, m_currPlaylistIndex, m_currTrackIndex);

		if (handlept == NULL)  // not in playlist either
		{
			sendStr << "115" << m_delimit << m_currPlaylistIndex
				<< m_delimit << m_currTrackIndex + 1
				<< m_delimit << "Track no longer exists in playlist" << m_delimit << "\r\n";
		}
		else
		{
			calculateTrackStateHeader(sendStr, 0);
			sendStr << m_delimit << m_currPlaylistIndex;
			sendStr << m_delimit << m_currTrackIndex + 1 << m_delimit;

			// fill in string with track information from playlist
			generateTrackString(handlept, sendStr, true);

		}
	}
	else
	{
		calculateTrackStateHeader(sendStr, 0);
		sendStr << m_delimit << m_currPlaylistIndex;
		if (m_currTrackIndex==pfc::infinite_size)
			sendStr << m_delimit << m_currTrackIndex << m_delimit;
		else
			sendStr << m_delimit << m_currTrackIndex + 1 << m_delimit;

		// fill in string with playing track information
		generateTrackString(pb_item_ptr, sendStr, true);
	}

    // now finally send the response
    sendData(clientSocket, sendStr);
}

// handle new connection
DWORD
WINAPI controlserver::ClientThread(void* cs)
{
    SOCKET clientSocket = reinterpret_cast<SOCKET>(cs);
    char dataBuf[m_bufferSize];
    WSAEVENT event;
    WSAEVENT events[2];
    WSANETWORKEVENTS networkEvents;
    int rcode = 0;
    int bytesRecv = 0;
    pfc::string8 recvCommand;
    pfc::string8 clientAddress = calculateSocketAddress(clientSocket);
    static_api_ptr_t<main_thread_callback_manager> cm;

    memset(dataBuf, 0, sizeof(dataBuf));

    // create event
    if((event = WSACreateEvent()) == NULL)
    {
		pfc::string8 msg;
		msg << "foo_controlserver: error " << WSAGetLastError() << " on client from " << clientAddress;
		console::warning(msg);
        shutdownClient(clientSocket);

        return 0;
    }

    events[0] = event;
    events[1] = m_WSAendEvent;

    // select the events we want to monitor
    if ((WSAEventSelect(clientSocket, event, FD_READ|FD_CLOSE)) == SOCKET_ERROR)
    {
		pfc::string8 msg;
		msg << "foo_controlserver: error " << WSAGetLastError() << " on client from " << clientAddress;
		msg << " WSAEventSelect()";
		console::warning(msg);
		
        shutdownClient(clientSocket);

        return 0;
    }

    // force update of track to this client only on connect
    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::trackupdateonconnect, clientSocket));

    // wait for events
    while (true)
    {
        if ((rcode = WSAWaitForMultipleEvents(
                    2,                                          // number of events
                    static_cast<const WSAEVENT FAR *>(events),  // the events
                    FALSE,                                      // don't wait for all events
                    10000,                                      // timeout of 10 seconds
                    FALSE))                                     // don't use a completion routine
                == WSA_WAIT_FAILED)
        {
			pfc::string8 msg;
			msg << "foo_controlserver: error " << WSAGetLastError() << " on client from " << clientAddress;
			msg << " WSAWaitForMultipleEvents()";
			console::warning(msg);
			
            break;
        } // end of if

        // check for timeout on wait, this is not an error if this occures, just continue on
        if (rcode == WSA_WAIT_TIMEOUT)
        {
            continue;
        }

        // check which event we received
        if (rcode == WSA_WAIT_EVENT_0)
        {
            // see which type of network event
            if ((WSAEnumNetworkEvents(clientSocket, event, &networkEvents))== SOCKET_ERROR)
            {
				pfc::string8 msg;
				msg << "foo_controlserver: error " << WSAGetLastError() << " on client from " << clientAddress;
				msg << " WSAEnumNetworkEvents()";
				console::warning(msg);
                break;
            }

            // check for a close event
            if (networkEvents.lNetworkEvents & FD_CLOSE)
            {
                // see if there was an error on the close event
                if (networkEvents.iErrorCode[FD_CLOSE_BIT] != 0)
                {
					pfc::string8 msg;
					msg << "foo_controlserver: error " << networkEvents.iErrorCode[FD_CLOSE_BIT] << " on client from " << clientAddress;
					msg << " FD_CLOSE";
					console::warning(msg);
                }

                // break out so exit can happen
                break;
            }

            // check for a read event
            if (networkEvents.lNetworkEvents & FD_READ)
            {
                // check for error
                if (networkEvents.iErrorCode[FD_READ_BIT] != 0)
                {
					pfc::string8 msg;
					msg << "foo_controlserver: error " << networkEvents.iErrorCode[FD_READ_BIT] << " on client from " << clientAddress;
					msg << " FD_READ";
					console::warning(msg);
					
                    // break out so exit can happen
                    break;
                }

                // clear buffer from previous time
                memset(dataBuf, 0, sizeof(dataBuf));

                // receive the data
                if ((bytesRecv = recv(clientSocket, dataBuf, sizeof(dataBuf), 0)) == SOCKET_ERROR)
                {
                    int lastError = WSAGetLastError();

                    if (lastError != WSAEWOULDBLOCK)
                    {
						pfc::string8 msg;
						msg << "foo_controlserver: error " << WSAGetLastError();
						msg << " on recv() on client from " << clientAddress;
						console::warning(msg);
						
                        break;
                    } else
                    {
//                        pfc::string8 msg;

//                        msg << "foo_controlserver: error " << WSAGetLastError();
//                        msg << " on recv() on client from " << clientAddress << "(WSAEWOULDBLOCK non-critical)";
//                        console::warning(msg);

                        continue;
                    }
                }

                if (bytesRecv == 0)
                {
                    break;  //indicates a nice close.
                }

                // add new received command to string
                recvCommand += dataBuf;

                // if we dont have an end of command char, keep going
                if (recvCommand.find_first("\n")==-1 && recvCommand.find_first("\r")==-1)
                {
                    continue;
                } else

                // now start looking for commands
                // did they ask to exit
                if (strncmp(recvCommand, "exit", 4) == 0)
                {
                    recvCommand = "";
                    break;
                } else

                // did they ask for the help command
                if (strncmp(recvCommand, "?", 1)==0)
                {
                    pfc::string8 temp(recvCommand);
                    temp.truncate_eol();

                    if (temp.length() == 1)
                    {
                        controlserver::handleHelpCommand(clientSocket, false);
                    }
                } else

                if (strncmp(recvCommand, "help", 4)==0)
                {
                    pfc::string8 temp(recvCommand);
                    temp.truncate_eol();

                    if (temp.length() == 4)
                    {
                        controlserver::handleHelpCommand(clientSocket, true);
                    }
                } else

                if (strncmp(recvCommand, "serverinfo", 10)==0)
                {
                    pfc::string8 temp(recvCommand);
                    temp.truncate_eol();

                    if (temp.length() == 10)
                    {
                        // done from this thread
                        controlserver::handleServerInfoCommand(clientSocket);
                    }
                } else

                // did they ask for track info
                if (strncmp(recvCommand, "trackinfo", 9) == 0)
                {
                    pfc::string8 temp(recvCommand);
                    temp.truncate_eol();

                    if (temp.length() == 9)
                    {
                        cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::trackinfo, clientSocket));
                    }
                } else

			    // get album art
			    if (strncmp(recvCommand, "albumart", 8) == 0)
			    {
						cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::albumart, clientSocket, recvCommand));
				}
				else

                // did they ask for list info
                if (strncmp(recvCommand, "listinfo", 8) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::listinfo, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "libsearch", 9) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::libsearch, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "order", 5) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::order, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "play", 4) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::play, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "vol", 3) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::volumeset, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "list", 4) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::list, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "queue", 5) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::queue, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "next", 4) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::next, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "prev", 4) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::prev, clientSocket, recvCommand));
                } else
                
                if (strncmp(recvCommand, "fooexit", 7) == 0 && !m_preventClose)
                {
                    //standard_commands::run_main(standard_commands::guid_main_exit);  
					standard_commands::run_main(standard_commands::guid_main_restart);	
                } else

                if (strncmp(recvCommand, "stop", 4) == 0)
                {
                    pfc::string8 temp(recvCommand);
                    temp.truncate_eol();

                    if (temp.length() == 4)
                    {
                        cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::stop, clientSocket));
                    }
                } else

                if (strncmp(recvCommand, "pause", 5) == 0)
                {
                    pfc::string8 temp(recvCommand);
                    temp.truncate_eol();

                    if (temp.length() == 5)
                    {
                        cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::pause, clientSocket));
                    }
                } else

                if (strncmp(recvCommand, "rand", 4) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::rand, clientSocket, recvCommand));
                } else

                if (strncmp(recvCommand, "seek", 4) == 0)
                {
                    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::seek, clientSocket, recvCommand));
                }

                // clear command, or junk line with linefeed/cr
                recvCommand = "";
            }
        } else
        {
            // we got an end event lets get out of here and gracefully close
            break;
        }
    } //end of while

    shutdownClient(clientSocket);

    return 0;
}

// used to get string address of socket
pfc::string8 const
controlserver::calculateSocketAddress(SOCKET clientSocket)
{
    pfc::string8 clientAddress;
    SOCKADDR_IN clientAddr;
    int clientAddrSize;

    // get size of structure
    clientAddrSize = sizeof(*(reinterpret_cast<SOCKADDR*>(&clientAddr)));

    // get the sockaddr structure
    if (getpeername(clientSocket, reinterpret_cast<SOCKADDR*>(&clientAddr), &clientAddrSize) == SOCKET_ERROR)
    {
		pfc::string8 msg;
		msg << "foo_controlserver: error " << WSAGetLastError() << " on getpeername()";
		console::warning(msg);
		
        clientAddress.reset();

        return clientAddress;
    }

    // convert to dotted quad string
    clientAddress = inet_ntoa(clientAddr.sin_addr);

    return clientAddress;
}

// close a client socket gracefully
void
controlserver::shutdownClient(SOCKET clientSocket)
{
    char dataBuf[m_bufferSize];
    pfc::string8 clientAddress(calculateSocketAddress(clientSocket));

	pfc::string8 msg;
	msg << "foo_controlserver: client from " << clientAddress << " disconnected";
	console::info(msg);

    std::vector<SOCKET>::iterator it;
    for (it=m_vclientSockets.begin(); it!= m_vclientSockets.end(); it++)
    {
        if (*it == clientSocket)
        {
            m_vclientSockets.erase(it);
            break;
        }
    }

    if (clientSocket != INVALID_SOCKET)
    {
        //send and receive FIN packets
        if (shutdown(clientSocket, 2) == SOCKET_ERROR)
        {
			pfc::string8 msg;
			msg << "foo_controlserver: error " << WSAGetLastError() << " on client from " << clientAddress;
			msg << " shutdown";
			console::warning(msg);	
        }

        if (recv(clientSocket, dataBuf, sizeof(dataBuf), 0) == SOCKET_ERROR)
        {
				//            pfc::string8 msg;
				//   we will get a WSAESHUTDOWN
				//            msg << "foo_controlserver: error " << WSAGetLastError() << " on recv() on client ";
				//            msg << clientAddress;
				//            console::warning(msg);
        }

        if (closesocket(clientSocket) == SOCKET_ERROR)
        {
			pfc::string8 msg;
			msg << "foo_controlserver: error " << WSAGetLastError() << " on closesocket() on client ";
			msg << clientAddress;
			console::warning(msg);
        }
    }
}

bool
controlserver::validateFuzzyMatch(pfc::string8 in, pfc::string8 fuzzy)
{
    char delim[] = ".";
    bool ok = true;
    int count = 0;
    pfc::string8 fuzzyQuad1;
    pfc::string8 fuzzyQuad2;
    pfc::string8 fuzzyQuad3;
    pfc::string8 fuzzyQuad4;
    pfc::string8 inQuad1;
    pfc::string8 inQuad2;
    pfc::string8 inQuad3;
    pfc::string8 inQuad4;

    char* f = strtok((char*)in.get_ptr(), delim);
    do
    {
        switch (count)
        {
        case 0 :
            inQuad1 = f;
            break;
        case 1 :
            inQuad2 = f;
        case 2 :
            inQuad3 = f;
        case 3 :
            inQuad4 = f;
        default :
            break;
        }

        count++;
        f = strtok(NULL, delim);
    }
    while (f != NULL);

    count = 0;

    f = strtok((char*)fuzzy.get_ptr(), delim);
    do
    {
        switch (count)
        {
        case 0 :
            fuzzyQuad1 = f;
            break;
        case 1 :
            fuzzyQuad2 = f;
        case 2 :
            fuzzyQuad3 = f;
        case 3 :
            fuzzyQuad4 = f;
        default :
            break;
        }

        count++;
        f = strtok(NULL, delim);
    }
    while (f != NULL);

    if (strcmp(fuzzyQuad1, "*") != 0)
    {
        if (strcmp(fuzzyQuad1, inQuad1) != 0)
        {
            ok = false;
        }
    }

    if (strcmp(fuzzyQuad2, "*") != 0)
    {
        if (strcmp(fuzzyQuad2, inQuad2) != 0)
        {
            ok = false;
        }
    }

    if (strcmp(fuzzyQuad3, "*") != 0)
    {
        if (strcmp(fuzzyQuad3, inQuad3) != 0)
        {
            ok = false;
        }
    }

    if (strcmp(fuzzyQuad4, "*") != 0)
    {
        if (strcmp(fuzzyQuad4, inQuad4) != 0)
        {
            ok = false;
        }
    }

    return ok;
}

bool
controlserver::validateConnectionMask(SOCKADDR_IN sock)
{
    char delim[] = ",";
    char* clientAddy = inet_ntoa(sock.sin_addr);
    pfc::string8 mask(m_connectionMask);
    std::vector<pfc::string8> m_connectionMasks;

    char* f = strtok((char*)mask.get_ptr(), delim);

    while (f != NULL)
    {
        m_connectionMasks.push_back(f);
        f = strtok(NULL, delim);
    }

    for (unsigned int i=0; i<m_connectionMasks.size(); i++)
    {
        if (strnicmp(m_connectionMasks.at(i), clientAddy,m_connectionMasks.at(i).length()) == 0)
        {
            return true;
        }

        if (validateFuzzyMatch(clientAddy, m_connectionMasks.at(i)))
        {
            return true;
        }
    }

    return false;
}

pfc::string8 controlserver::getLocalHostIP()
{
	char host[256];
	pfc::string8 msg = "";
	pfc::string8 error_msg = "";

	WSADATA            wsd;
	// start up winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsd) == 0)
	{
		if (wsd.wVersion == MAKEWORD(2, 2))
		{

			if (gethostname(host, sizeof(host)) == SOCKET_ERROR) 
			{
				error_msg << "no host info - error " << WSAGetLastError() << " when getting host";
			}
			else
			{
				//msg << host;

				struct hostent *phosts = gethostbyname(host);
				if (phosts == 0) 
				{
					error_msg << " Bad host lookup";
				}
				else
				{

					for (int i = 0; phosts->h_addr_list[i] != 0; ++i)
					{
						struct in_addr addr;
						memcpy(&addr, phosts->h_addr_list[i], sizeof(struct in_addr));
						msg << inet_ntoa(addr) << "  ";
					}
				}

			}

			WSACleanup();
		}
		else
		{
			error_msg = "no host info - error opening network";
		}
	}
	else
	{
		error_msg = "no host info - error opening network";
	}

	if (error_msg.get_length() > 0)
	{
		return error_msg;
	}
	else
	{
		return msg;
	}
}


// start up the server and listen for connections/spawn client handling threads
void
controlserver::start(int port, HANDLE endEvent)
{
    WSADATA wsd;
    SOCKET sListen;     // socket we are going to listen for connections on
    SOCKET sClient;     // socket we are giving the client if accept it
    SOCKADDR_IN client;
    SOCKADDR_IN local;
    WSAEVENT hEvent;
    WSANETWORKEVENTS events;
    HANDLE hThread = NULL;
    DWORD dwThreadId = 0;
    int nRet = 0;
    char dataBuf[m_bufferSize];
    static_api_ptr_t<main_thread_callback_manager> cm;

    // start up winsock
    if (WSAStartup(MAKEWORD(2,2), &wsd) != 0)
    {
		pfc::string8 str;

		str << "foo_controlserver: error" << WSAGetLastError() << " on server WSAStartup";
		console::warning(str);
		
        return;
    }

    // make sure version is ok
    if (wsd.wVersion != MAKEWORD(2, 2))
    {
        console::warning("foo_controlserver: could not find the requested winsock version");

        return;
    }

    // create out listening socket
    sListen = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, NULL);
    if (sListen == INVALID_SOCKET)
    {
        pfc::string8 str;

        str << "foo_controlserver: error " << WSAGetLastError() << " on server WSASocket()";
        console::warning(str);

        return;
    }

    // setup address
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_family = AF_INET;
    local.sin_port = htons(port);

    // bind to address/port
    if (bind(sListen, reinterpret_cast<SOCKADDR*>(&local), sizeof(local)) == SOCKET_ERROR)
    {
        pfc::string8 msg;

        msg << "foo_controlserver: error " << WSAGetLastError() << " on server bind()";
        console::warning(msg);

        return;
    }

    // listen for connections, 4 backlog
    listen(sListen, 4);

    // create wsa event
    hEvent = WSACreateEvent();
    if (hEvent == WSA_INVALID_EVENT)
    {
        pfc::string8 msg;

        msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSACreateEvent()";
        console::warning(msg);

        if (closesocket(sListen) == SOCKET_ERROR)
        {
            pfc::string8 msg;

            msg << "foo_controlserver: error " << WSAGetLastError() << " on server closesocket()";
            console::warning(msg);
        }

        return;
    }

    // create event
    if((m_WSAendEvent = WSACreateEvent()) == NULL)
    {
        pfc::string8 msg;

        msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSACreateEvent()";
        console::warning(msg);

        if (closesocket(sListen) == SOCKET_ERROR)
        {
            pfc::string8 msg;

            msg << "foo_controlserver: error " << WSAGetLastError() << " on server closesocket()";
            console::warning(msg);
        }

        return;
    }

    nRet = WSAEventSelect(sListen, hEvent, FD_ACCEPT);
    if (nRet == SOCKET_ERROR)
    {
        pfc::string8 msg;

        msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSAEventSelect()";
        console::warning(msg);

        if (closesocket(sListen) == SOCKET_ERROR)
        {
            pfc::string8 msg;

            msg << "foo_controlserver: error " << WSAGetLastError() << " on server closesocket()";
            console::warning(msg);
        }

        if (!WSACloseEvent(hEvent))
        {
            pfc::string8 msg;

            msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSACloseEvent()";
            console::warning(msg);
        }

        return;
    }

	pfc::string8 msg;
	msg << "This PC : " << controlserver::getLocalHostIP();
	console::info(msg);
	msg = "";
    msg << "foo_controlserver: control server up and running on port " << port;
    console::info(msg);

	

    // force update of track infos
    cm->add_callback(new service_impl_t<CHandleCallbackRun>(CHandleCallbackRun::inittrackers));

    // Handle async network events
    while(1)
    {
        // Wait for something to happen
        DWORD dwRet, normRet;

        normRet = WaitForMultipleObjects(1, &endEvent, FALSE, 100);

        if (normRet == WAIT_FAILED)
        {
            pfc::string8 msg;

            msg << "foo_controlserver: error " << WSAGetLastError() << " on server WaitForMultipleObjects()";
            console::warning(msg);

            break;
        }

        // we got an exit event from the main thread, lets get out of here
        if (normRet == WAIT_OBJECT_0)
        {
            // signal clients to end
            WSASetEvent(m_WSAendEvent);
            break;
        }

        // wait for network events
        dwRet = WSAWaitForMultipleEvents(1, &hEvent, FALSE, 100, FALSE);

        if (dwRet == WSA_WAIT_FAILED)
        {
            pfc::string8 msg;

            msg << "foo_controlserver: error " << WSAGetLastError() << " on server WaitForMultipleEvents()";
            console::warning(msg);

            break;
        } else if (dwRet == WSA_WAIT_TIMEOUT)
        {
            // timed out, no worries, just keep going
            continue;
        } else if (dwRet == WSA_WAIT_EVENT_0)
        {
            // got a network event we wanted
            nRet = WSAEnumNetworkEvents(sListen, hEvent, &events);

            if (nRet == SOCKET_ERROR)
            {
                pfc::string8 msg;

                msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSAEnumNetworkEvents()";
                console::warning(msg);

                break;
            }

            // Handle events
            if (events.lNetworkEvents & FD_ACCEPT)
            {
                pfc::string8 msg;
                pfc::string8 sendStr;

                int iAddrSize = sizeof(client);

                sClient = WSAAccept(sListen, reinterpret_cast<SOCKADDR*>(&client), &iAddrSize, NULL, NULL);

                if (sClient == INVALID_SOCKET)
                {
                    pfc::string8 msg;

                    msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSAAccept()";
                    console::warning(msg);

                    break;
                }

                if (!validateConnectionMask(client))
                {
                    pfc::string8 msg;

                    msg << "foo_controlserver: rejected connection from " << inet_ntoa(client.sin_addr) << "\r\n";
                    console::warning(msg);

                    //send and receive FIN packets
                    if (shutdown(sClient, 2) == SOCKET_ERROR)
                    {
                        pfc::string8 msg;

                        msg << "foo_controlserver: error " << WSAGetLastError() << " on server shutdown()";
                        console::warning(msg);
                    }

                    if (recv(sClient, dataBuf, sizeof(dataBuf), 0) == SOCKET_ERROR)
                    {
                        // we will get WSAESHUTDOWN
//                        pfc::string8 msg;

//                        msg << "foo_controlserver: error " << WSAGetLastError() << " on server recv()";
//                        console::warning(msg);
                    }

                    if (closesocket(sClient) == SOCKET_ERROR)
                    {
                        pfc::string8 msg;

                        msg << "foo_controlserver: error " << WSAGetLastError() << " on server closesocket()";
                        console::warning(msg);
                    }

                    continue;
                }

                sendStr << "999" << m_delimit << "Connected to foobar2000 Control Server " <<
                            m_versionNumber << m_delimit << "\r\n" <<
                       "999" << m_delimit << "Accepted client from " << inet_ntoa(client.sin_addr) <<
                            m_delimit << "\r\n" <<
                       "999" << m_delimit << "There are currently " << m_vclientSockets.size()+1 << "/" <<
                            m_maxClients << " clients connected" << m_delimit << "\r\n";


                msg << "foo_controlserver: accepted client from " << inet_ntoa(client.sin_addr) << "\r\n";
                console::info(msg);

                if (m_vclientSockets.size()+1 > m_maxClients)
                {
                    pfc::string8 msg;

                    sendStr << "999" << m_delimit << "The server is full. Try again later." << m_delimit << "\r\n";

                    msg << "foo_controlserver: rejected client from " << inet_ntoa(client.sin_addr) << "; the server is full.";
                    console::info(msg);

                    sendData(sClient, sendStr);

                    //send and receive FIN packets
                    if (shutdown(sClient, 2) == SOCKET_ERROR)
                    {
                        pfc::string8 msg;

                        msg << "foo_controlserver: error " << WSAGetLastError() << " on server shutdown()";
                        console::warning(msg);
                    }

                    if (recv(sClient, dataBuf, sizeof(dataBuf), 0) == SOCKET_ERROR)
                    {
                        // we will get WSAESHUTDOWN
//                        pfc::string8 msg;

//                        msg << "foo_controlserver: error " << WSAGetLastError() << " on server recv()";
//                        console::warning(msg);
                    }

                    if (closesocket(sClient) == SOCKET_ERROR)
                    {
                        pfc::string8 msg;

                        msg << "foo_controlserver: error " << WSAGetLastError() << " on server closesocket()";
                        console::warning(msg);
                    }

                    continue;
                }
                else
                {
                    sendStr << "999" << m_delimit << "Type '?' or 'help' for command information";
                    sendStr << m_delimit << "\r\n";
                }

                sendData(sClient, sendStr);

                // create thread to handle this client
                hThread = CreateThread(NULL, 0, ClientThread, reinterpret_cast<LPVOID>(sClient), 0, &dwThreadId);

                if (hThread == NULL)
                {
                    console::warning("foo_controlserver: could not create thread to handle an incoming connection");
                    break;
                }

                m_vclientSockets.push_back(sClient);
            }
        }
    }

    if (closesocket(sListen) == SOCKET_ERROR)
    {
        pfc::string8 msg;

        msg << "foo_controlserver: error " << WSAGetLastError() << " on server closesocket()";
        console::warning(msg);
    }

    if (WSACleanup() == SOCKET_ERROR)
    {
        pfc::string8 msg;

        msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSACleanup()";
        console::warning(msg);
    }

    if (!WSACloseEvent(m_WSAendEvent))
    {
        pfc::string8 msg;

        msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSACloseEvent()";
        console::warning(msg);
    }

    if (!WSACloseEvent(hEvent))
    {
        pfc::string8 msg;

        msg << "foo_controlserver: error " << WSAGetLastError() << " on server WSACloseEvent()";
        console::warning(msg);
    }

    console::info("foo_controlserver: control server has been brought down");

    return;
}
