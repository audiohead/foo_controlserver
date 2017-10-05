/*
 * Nov 2016 - added this file for new album art function - Walter Hartman
 */
#pragma once
#include "../../SDK/foobar2000.h"
#include "../../../pfc/pfc.h"

class albumart
{
public:
	enum albumart_status {
		AS_NO_INFO = 0,	    // no playing item so no albumart info
		AS_FOUND = 1,	    // albumart found
		AS_NOT_FOUND = 2,	// albumart not found anywhere
	};

	albumart_status pb_albumart_status;

	int numBlocks;
	int size;
	pfc::string8 trackTitle;   // track title for this album art
	pfc::string8 albumTitle;   // album title
	pfc::string8 base64string; // return base 64 string of jpg or png album art

public:
	albumart();
	~albumart();

	void reset();

};

