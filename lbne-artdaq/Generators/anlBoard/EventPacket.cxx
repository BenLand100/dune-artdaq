#include "EventPacket.h"

void SSPDAQ::EventPacket::SetEmpty(){
  data.clear();
  header.header=0xDEADBEEF;
}

/*SSPDAQ::Event* SSPDAQ::EventPacket::Unpack() {
	int i = 0;
	int error = 0;
	unsigned short waveformLength = 0;
	unsigned short packetLength = 0;
	SSPDAQ::Event* event=new Event;

	// Calculate lengths
	packetLength = this->header.length;		// Packet in unsigned ints 
	packetLength = packetLength * sizeof(unsigned int);	// Packet in bytes
	
	if (packetLength < sizeof(EventHeader)) {
		error = errorEventTooSmall;
	} else {
		waveformLength = packetLength - sizeof(EventHeader);	// Waveform in bytes
		waveformLength = waveformLength >> 1;					// Waveform in shorts
		
		if (waveformLength > MAX_EVENT_DATA) {
			error = errorEventTooLarge;
		}
	}
	
	if (error) {
		// Return empty event structure
		event->header			= 0;
		event->packetLength		= 0;
		event->triggerType		= 0;
		event->status.flags		= 0;
		event->headerType		= 0;
		event->triggerID		= 0;
		event->moduleID			= 0;
		event->channelID		= 0;
		event->syncDelay		= 0;
		event->syncCount		= 0;
		event->peakSum			= 0;
		event->peakTime			= 0;
		event->prerise			= 0;
		event->integratedSum	= 0;
		event->baseline			= 0;
		event->cfdPoint[0]		= 0;
		event->cfdPoint[1]		= 0;
		event->cfdPoint[2]		= 0;
		event->cfdPoint[3]		= 0;
		event->intTimestamp[0]	= 0;
		event->intTimestamp[1]	= 0;
		event->intTimestamp[2]	= 0;
		event->intTimestamp[3]	= 0;
		event->waveformWords	= 0;
		
		for (i = 0; i < MAX_EVENT_DATA; i++) {
			event->waveform[i] = 0;
		}
	} else {
		// Unpack packet into event structure
		event->header			= this->header.header;
		event->packetLength		= this->header.length;
		event->triggerType		= (this->header.group1 & 0xFF00) >> 8;
		event->status.flags		= (this->header.group1 & 0x00F0) >> 4;
		event->headerType		= (this->header.group1 & 0x000F) >> 0;
		event->triggerID		= this->header.triggerID;
		event->moduleID			= (this->header.group2 & 0xFFF0) >> 4;
		event->channelID		= (this->header.group2 & 0x000F) >> 0;
		event->syncDelay		= ((unsigned int)(this->header.timestamp[1]) << 16) + (unsigned int)(this->header.timestamp[0]);
		event->syncCount		= ((unsigned int)(this->header.timestamp[3]) << 16) + (unsigned int)(this->header.timestamp[2]);
		// PeakSum arrives as a signed 24 bit number.  It must be sign extended when stored as a 32 bit int
		event->peakSum			= ((this->header.group3 & 0x00FF) << 16) + this->header.peakSumLow;
		if (event->peakSum & 0x00800000) {
			event->peakSum |= 0xFF000000;
		}
		event->peakTime			= (this->header.group3 & 0xFF00) >> 8;
		event->prerise			= ((this->header.group4 & 0x00FF) << 16) + this->header.preriseLow;
		event->integratedSum	= ((unsigned int)(this->header.intSumHigh) << 8) + (((unsigned int)(this->header.group4) & 0xFF00) >> 8);
		event->baseline			= this->header.baseline;
		event->cfdPoint[0]		= this->header.cfdPoint[0];
		event->cfdPoint[1]		= this->header.cfdPoint[1];
		event->cfdPoint[2]		= this->header.cfdPoint[2];
		event->cfdPoint[3]		= this->header.cfdPoint[3];
		event->intTimestamp[0]	= this->header.intTimestamp[0];
		event->intTimestamp[1]	= this->header.intTimestamp[1];
		event->intTimestamp[2]	= this->header.intTimestamp[2];
		event->intTimestamp[3]	= this->header.intTimestamp[3];
		event->waveformWords	= waveformLength;

		// Copy waveform into event structure
		for (i = 0; i < event->waveformWords; i++) {
			event->waveform[i] = this->waveform[i];
		}
		// Fill rest of waveform storage with zero
		for (; i < MAX_EVENT_DATA; i++) {
			event->waveform[i] = 0;
		}
	}
	
	return event;
	}*/
