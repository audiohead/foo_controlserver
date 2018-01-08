/*
* Sept 2017 - added this file for new album art function - Walter Hartman
*/

#include "albumart.h"

albumArt::albumArt()
{
	reset();
}

albumArt::~albumArt()
{
}


void albumArt::reset(void)
{
	numBlocks = 0;
	imageSize = 0;
	base64Size = 0;
	pb_albumart_status = AS_NO_INFO;

	timeStampTicks = 0;

	trackTitle.reset();
	albumTitle.reset();
	base64String.reset();
}
