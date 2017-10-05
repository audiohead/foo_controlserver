/*
* Nov 2016 - added this file for new album art function - Walter Hartman
*/

#include "albumart.h"

albumart::albumart()
{
	reset();
}

albumart::~albumart()
{
}


void albumart::reset(void)
{
	numBlocks = 0;
	size = 0;

	trackTitle.reset();
	base64string.reset();
}
