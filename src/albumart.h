/*
 * Sept 2017 - added this file for new album art function - Walter Hartman
 */
#pragma once
#include "../../SDK/foobar2000.h"
#include "../../../pfc/pfc.h"

class albumArt
{
public:
	enum albumArtStatus {
		AS_NO_INFO = 0,	    // no playing item so no albumart info
		AS_FOUND = 1,	    // albumart found
		AS_NOT_FOUND = 2,	// albumart not found anywhere
	};

	albumArtStatus pb_albumart_status;
	DWORD timeStampTicks; // in millisecs since system start

	int numBlocks;
	int imageSize;
	int base64Size;
	pfc::string8 trackTitle;   // track title for this album art
	pfc::string8 albumTitle;   // album title
	pfc::string8 base64String; // return base 64 string of jpg or png album art


public:
	albumArt();
	~albumArt();

	void reset();

};

