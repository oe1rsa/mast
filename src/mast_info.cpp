/*
 *  MAST: Multicast Audio Streaming Toolkit
 *
 *  Copyright (C) 2003-2007 Nicholas J. Humfrey <njh@aelius.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  $Id$
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

#include "MastTool.h"
#include "MPA_Header.h"
#include "mast.h"


#define MAST_TOOL_NAME	"mast_info"



static int usage() {
	
	printf( "Multicast Audio Streaming Toolkit (version %s)\n", PACKAGE_VERSION);
	printf( "%s <address>[/<port>]\n", MAST_TOOL_NAME);
//	printf( "    -s <ssrc>     Source identifier (otherwise use first recieved)\n");
	
	exit(1);
	
}


static void parse_cmd_line(int argc, char **argv, RtpSession* session)
{
	char* local_address = NULL;
	int local_port = DEFAULT_RTP_PORT;
	int ch;


	// Parse the options/switches
	while ((ch = getopt(argc, argv, "h?")) != -1)
	switch (ch) {
		// case 'T': FIXME: timeout?
		case '?':
		case 'h':
		default:
			usage();
	}


	// Parse the ip address and port
	if (argc > optind) {
		local_address = argv[optind];
		optind++;
		
		// Look for port in the address
		char* portstr = strchr(local_address, '/');
		if (portstr && strlen(portstr)>1) {
			*portstr = 0;
			portstr++;
			local_port = atoi(portstr);
		}
	
	} else {
		MAST_ERROR("missing address/port to receive from");
		usage();
	}
	
	// Make sure the port number is even
	if (local_port%2 == 1) local_port--;
	
	// Set the remote address/port
    if (rtp_session_set_local_addr( session, local_address, local_port, local_port+1 )) {
		MAST_FATAL("Failed to set receive address/port (%s/%u)", local_address, local_port);
	} else {
		printf( "Receive address: %s/%u\n", local_address,  local_port );
	}
	
}





int main(int argc, char **argv)
{
	MastTool* tool = NULL;
	mblk_t* packet = NULL;
	PayloadType* pt = NULL;
	int payload_size = 0;
    unsigned char* payload = NULL;
	
	// Create an RTP session
	tool = new MastTool( MAST_TOOL_NAME, RTP_SESSION_RECVONLY );
	if (tool==NULL) return -1;

	// Parse the command line arguments 
	// and configure the session
	parse_cmd_line( argc, argv, tool->get_session() );

	

	// Setup signal handlers
	mast_setup_signals();

	// Recieve a single packet
	packet = tool->wait_for_rtp_packet();
	if (packet == NULL) MAST_FATAL("Failed to receive a packet");
    payload_size = rtp_get_payload( packet, &payload);

	
	// Display information about the packet received
	printf("\n");
	printf("RTP Header\n");
	printf("==========\n");
	printf("Payload type    : %u\n", rtp_get_payload_type( packet ) );
	printf("Payload size    : %u bytes\n", payload_size );
	printf("Sequence Number : %u\n", rtp_get_seqnumber( packet ) );
	printf("Timestamp       : %u\n", rtp_get_timestamp( packet ) );
	printf("SSRC Identifier : %x\n", rtp_get_ssrc( packet ) );
	printf("Marker Bit      : %s\n", rtp_get_markbit( packet ) ? "Set" : "Not Set");
	printf("\n");
	

	// Lookup the payload type
	pt = rtp_profile_get_payload( tool->get_profile(), rtp_get_payload_type( packet ) );
	if (pt == NULL) {
		MAST_WARNING( "Payload type %u isn't registered with oRTP", rtp_get_payload_type( packet ) );
	} else {
		const char* mime_major = "?";

		printf("Payload Details\n");
		printf("===============\n");

		if (pt->type==PAYLOAD_AUDIO_CONTINUOUS) mime_major = "audio";
		else if (pt->type==PAYLOAD_AUDIO_PACKETIZED) mime_major = "audio";
		else if (pt->type==PAYLOAD_VIDEO) mime_major = "video";
		printf("Mime Type       : %s/%s\n", mime_major, pt->mime_type);
		
		if (pt->clock_rate)			printf("Clock Rate      : %u Hz\n", pt->clock_rate);
		if (pt->channels)			printf("Channels        : %u\n", pt->channels);
		if (pt->bits_per_sample)	printf("Bits per Sample : %u\n", pt->bits_per_sample);
		if (pt->normal_bitrate) {
			printf("Normal Bitrate  : %u kbps\n", (pt->normal_bitrate/1000));
			printf("Packet duration : %u ms\n", (payload_size*1000)/(pt->normal_bitrate/8) );
		}
		if (pt->recv_fmtp)			printf("Recieve FMTP    : %s\n", pt->recv_fmtp);
		if (pt->send_fmtp)			printf("Send FMTP       : %s\n", pt->send_fmtp);
		printf("\n");
		
	
	}


	// Parse the MPEG Audio header
	if (rtp_get_payload_type( packet ) == RTP_MPEG_AUDIO_PT) {
		/* FIXME: check fragment offset header (see rfc2250) */
        unsigned char* mpa_ptr = payload + 4;
		MPA_Header mh;
	
		printf("MPEG Audio Header\n");
		printf("=================\n");
		
        if (!mh.parse( mpa_ptr )) {
			MAST_WARNING("Failed to parse MPEG Audio header");
		} else {
			mh.debug( stdout );
		}
	}
	

	// Close RTP session
	delete tool;
	
	 
	// Success !
	return 0;
}


