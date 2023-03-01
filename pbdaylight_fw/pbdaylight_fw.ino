#include <Arduino.h>
#include <util/delay.h>

extern "C" {
#include "usbdrv.h"
}

struct ReportMessage {
	float targetBrightness;
	float transitionDuration;
};

unsigned long currentMillis = 0, transitionStartMillis = 0, transitionEndMillis = 0;
float transitionStartBrightness = 0, currentBrightness = 0, transitionEndBrightness = 0;
bool ledState = false;

void setup()
{
	// Pins 0 and 1 set as outputs
	pinMode(0, OUTPUT);
  pinMode(1, OUTPUT);

	// Setup USB
	{
		cli();

		usbInit();
		usbDeviceDisconnect();

		// fake USB disconnect for > 250 ms - required in example?
		delay(300);

		usbDeviceConnect();
		
		sei();
	}

	// Notify that the ring is running
	for(int i = 0; i < 2; i++) {
		analogWrite(0, 64);
    digitalWrite(1, HIGH);
		delay(500);
		analogWrite(0, 0);
    digitalWrite(1, LOW);
    delay(500);
	}
}

void loop()
{
	currentMillis = millis();

	usbPoll();

	if(currentMillis <= transitionEndMillis) {
		const float t = float(currentMillis - transitionStartMillis) / float(transitionEndMillis - transitionStartMillis);
		currentBrightness = transitionStartBrightness * (1 - t) + transitionEndBrightness * t;
	}
	else
		currentBrightness = transitionEndBrightness;

	analogWrite(0, int(currentBrightness * 255));
}

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = reinterpret_cast<usbRequest_t *>(data);

	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_CLASS)
		return 0;

	if (rq->bRequest == USBRQ_HID_SET_REPORT)
	{
		return USB_NO_MSG;
	}

	return 0;
}

uint8_t usbFunctionWrite(uint8_t *data, uint8_t len)
{
	/*if (len == 0 || !isExpectingMessage)
		return !isExpectingMessage;*/

	const ReportMessage &msg = *reinterpret_cast<ReportMessage*>(data);

	transitionStartBrightness = currentBrightness;
	transitionStartMillis = currentMillis;

	transitionEndMillis = currentMillis + static_cast<unsigned long>(msg.transitionDuration * 1000);
	transitionEndBrightness = msg.targetBrightness;

 ledState ^= 1;

	// We've received all data, thanks
	return 1;
}

PROGMEM const char usbHidReportDescriptor[22] = {
		0x06, 0x00, 0xff, // USAGE_PAGE (Generic Desktop)
		0x09, 0x01,				// USAGE (Vendor Usage 1)
		0xa1, 0x01,				// COLLECTION (Application)
		0x15, 0x00,				//   LOGICAL_MINIMUM (0)
		0x26, 0xff, 0x00, //   LOGICAL_MAXIMUM (255)
		0x75, 0x08,				//   REPORT_SIZE (8)
		0x95, 0x04,				//   REPORT_COUNT (1)
		0x09, 0x00,				//   USAGE (Undefined)
		0xb2, 0x02, 0x01, //   FEATURE (Data,Var,Abs,Buf)
		0xc0							// END_COLLECTION
};
