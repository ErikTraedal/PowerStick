/*
PowerStick, a USB hardware that turns any media player with at least 60 songs in a playlist into a power hour machine.
Copyright (C) 2014 Erik Trædal

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h> 
#include <avr/pgmspace.h>

#include "usbdrv.h"

#define ledRedOn()    PORTC &= ~(1 << PC1)
#define ledRedOff()   PORTC |= (1 << PC1)
#define ledGreenOn()  PORTC &= ~(1 << PC0)
#define ledGreenOff() PORTC |= (1 << PC0)

#define NONE						0x00
#define CONSUMER_SCAN_NEXT_TRACK	0xb5
#define CONSUMER_PLAY_PAUSE			0xcd

static uchar idleRate;
static uchar reportBuffer[2];

static uchar buildReport(uchar key) {
	// Raport ID
	reportBuffer[0] = 1;

	// Our key
	if (key == CONSUMER_SCAN_NEXT_TRACK) {
		reportBuffer[1] = 0x01;
	} else if (key == CONSUMER_PLAY_PAUSE) {
		reportBuffer[1] = 0x02;
	}

	return sizeof(reportBuffer);
}

// Everyone seem to be generating this with a tool from usb.org, so I wanted to try to do it manually
// to make sure I understand everything. In other words, expect there to be errors
PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] =
		{ 0x05, 0x0c, 					// Usage switch, generic desktop
				0x09, 0x01, 					// Usage, consumer control
				0xa1, 0x01, 					// Collection, application
				0x85, 0x01,						// Raport ID, 1
				0x15, 0x00,						// Logical miniumum, 0
				0x25, 0x01,						// Logical maximum, 1
				0x09, CONSUMER_SCAN_NEXT_TRACK,	// Usage, key
				0x09, CONSUMER_PLAY_PAUSE,		// Usage, key
				0x75, 0x01,						// Report size, 1
				0x95, 0x02,						// Report count, 2
				0x81, 0x06,						// Input
				0xc0 };

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
	usbRequest_t *rq = (void *) data;

	usbMsgPtr = reportBuffer;
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
		if (rq->bRequest == USBRQ_HID_GET_REPORT) {
			return buildReport(0);
		} else if (rq->bRequest == USBRQ_HID_GET_IDLE) {
			usbMsgPtr = &idleRate;

			return 1;
		} else if (rq->bRequest == USBRQ_HID_SET_IDLE) {
			idleRate = rq->wValue.bytes[1];
		}
	}

	return 0;
}

int __attribute__((noreturn)) main(void) {
	uchar keyToSend = 0;
	uchar statusLed = 0;
	uchar smallCounter = 0;
	uchar secondCounter = 0;
	uchar minutesCounter = 0;
	uchar i;

	wdt_enable (WDTO_1S);

	// All ports inputs, except PC0, PC1
	PORTC = 0x00;
	DDRC = 0x03;

	// From v-usb mouse example
	usbInit();
	usbDeviceDisconnect(); /* enforce re-enumeration, do this while interrupts are disabled! */
	i = 0;
	while (--i) { /* fake USB disconnect for > 250 ms */
		wdt_reset();
		_delay_ms(1);
	}

	usbDeviceConnect();
	sei();

	// Timer 0 1024 divisor
	TCCR0 = 5;

	ledGreenOff();

	for (;;) {
		wdt_reset();
		usbPoll();

		if (TIFR & (1 << TOV0)) {
			TIFR = 1 << TOV0;

			// This triggers roughly every second
			if (++smallCounter > 46) {
				smallCounter = 0;

				// Toggle the red led, so we know things work
				if (statusLed++ > 0) {
					statusLed = 0;
					ledRedOn();
				} else {
					ledRedOff();
				}

				// See if it's time to send our key
				if (++secondCounter >= 60) {
					secondCounter = 0;
					keyToSend = CONSUMER_SCAN_NEXT_TRACK;

					if (++minutesCounter >= 60) {
						// Finished, time to stop
						keyToSend = CONSUMER_PLAY_PAUSE;
						ledGreenOn();
						for(;;);
					}
				}
			}
		}

		if (keyToSend && usbInterruptIsReady()) {
			usbSetInterrupt(reportBuffer, buildReport(keyToSend));
			keyToSend = 0;
		}
	}
}
