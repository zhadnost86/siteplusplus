#include "stdafx.h"
#include <string.h>
#include "socketio.h"

void TSocketIO::readstr( char *buf )
{
	int r;
	char ch;

	buf[0] = 0;

	while (true)
	{
		if ((r = recvData( &ch, 1, 8 /* MSG_WAITALL */ )) <= 0)
			return;

		if (ch == '\r')
		{
			r = recvData( &ch, 1, 8 );
			break;
		}
		if (ch == '\n')
			break;

		*(buf++) = ch;
	}

	*buf = 0;
}

void TSocketIO::writestr( char *buf )
{
	char bufp[]="\r\n";

	sendData( buf, strlen(buf) );
	sendData( &bufp[0], 2 );
}

char* alloccpy( char *buf )
{
	char *s = (char*) malloc( strlen( (char*) buf)+1 );
	strcpy( s, (char*) buf );
	return s;
}

char *skipLeadingSpaces( char *s )
{
	while (*s == ' ')
		s++;
	return s;
}

int TSocketIO::recvData( char *buf, int size, int flags )
{
	return ::recv( sock,buf, size, flags );
}

void TSocketIO::sendData( char *buf, int size, int flags )
{
	::send( sock, buf, size, flags );
}
