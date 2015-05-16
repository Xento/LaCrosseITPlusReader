// Tested with JeeLink v3 (2012-02-11)
// polling RFM12B to decode FSK iT+ with a JeeNode/JeeLink from Jeelabs.
// Supported devices: see FHEM wiki
// info    : http://forum.jeelabs.net/node/110
//           http://fredboboss.free.fr/tx29/tx29_sw.php
//           http://www.f6fbb.org/domo/sensors/
//           http://www.mikrocontroller.net/topic/67273
//           benedikt.k org rinie,marf,joop 1 nov 2011, slightly modified by Rufik (r.markiewicz@gmail.com)
// Changelog: 2012-02-11: initial release 1.0
//            2014-03-14: I have this in SubVersion, so no need to do it here

//// http://forum.fhem.de/index.php/topic,14786.msg268544.html#msg268544

#define PROGNAME         "LaCrosseWh1080ITPlusReader"
#define PROGVERS         "00.1h"

#include "RFMxx.h"
#include "SensorBase.h"
#include "LaCrosse.h"
#include "LevelSenderLib.h"
#include "EMT7110.h"
#include "WT440XH.h"
#include "TX38IT.h"
#include "WH1080.h"
#include "WS1600.h"
#include <Time.h>
#include "JeeLink.h"
#include "Transmitter.h"
#include "Help.h"

// --- Configuration ---------------------------------------------------------
#define RECEIVER_ENABLED      1                     // Set to 0 if you don't want to receive
#define ANALYZE_FRAMES        0                     // Set to 1 to display analyzed frame data instead of the normal data
bool fOnlyIfValid           = true;
bool fFhemDisplay           = false;                // set to false for text display
#define ENABLE_ACTIVITY_LED   1                     // set to 0 if the blue LED bothers
#define USE_OLD_IDS           0                     // Set to 1 to use the old ID calcualtion
// The following settings can also be set from FHEM
bool    DEBUG               = 0;                    // set to 1 to see debug messages
unsigned long DATA_RATE     = 17241ul;              // use one of the possible data rates
uint16_t TOGGLE_DATA_RATE   = 30;                    // 0=no toggle, else interval in seconds
unsigned long lastWh1080 = 0;						// 48 seconds so try 60 seconds first...
unsigned long INITIAL_FREQ  = 868300;               // Initial frequency in kHz (5 kHz steps, 860480 ... 879515)
bool RELAY                  = 0;                    // If 1 all received packets will be retransmitted


// --- Variables --------------------------------------------------------------
unsigned long lastToggle = 0;
byte commandData[32];
byte commandDataPointer = 0;
RFMxx rfm(11, 12, 13, 10, 2);
JeeLink jeeLink;
Transmitter transmitter(&rfm);


static void HandleSerialPort(char c) {
  static unsigned long value;

  if (c == ',') {
    commandData[commandDataPointer++] = value;
    value = 0;
  }
  else if ('0' <= c && c <= '9') {
    value = 10 * value + c - '0';
  }
  else if ('a' <= c && c <= 'z') {
    switch (c) {
    case 'd':
      // DEBUG
      SetDebugMode(value);
      break;
    case 'x':
      // Tests
      HandleCommandX(value);
      break;
    case 'a':
      // Activity LED
      jeeLink.EnableLED(value);
      break;
    case 'r':
      // Data rate
      DATA_RATE = value ? 9579ul : 17241ul;
      rfm.SetDataRate(DATA_RATE);
      break;
    case 't':
      // Toggle data rate
      TOGGLE_DATA_RATE = value;
      break;
    case 'v':
      // Version info
      HandleCommandV();
      break;
    case 's':
      // Send
      commandData[commandDataPointer] = value;
      HandleCommandS(commandData, ++commandDataPointer);
      commandDataPointer = 0;
      break;

    case 'i':
      commandData[commandDataPointer] = value;
      HandleCommandI(commandData, ++commandDataPointer);
      commandDataPointer = 0;
      break;

    case 'c':
      commandData[commandDataPointer] = value;
      HandleCommandC(commandData, ++commandDataPointer);
      commandDataPointer = 0;
      break;

    case 'f':
      rfm.SetFrequency(value);
      break;

    case 'y':
      RELAY = value;
      break;

    default:
      HandleCommandV();
      Help::Show();
      break;
    }
    value = 0;
  }
  else if (' ' < c && c < 'A') {
    HandleCommandV();
    Help::Show();
  }
}

void SetDebugMode(boolean mode) {
  DEBUG = mode;
  LevelSenderLib::SetDebugMode(mode);
  WT440XH::SetDebugMode(mode);
  rfm.SetDebugMode(mode);
}

void HandleCommandS(byte *data, byte size) {
  if (size == 4){
    rfm.EnableReceiver(false);

    // Calculate the CRC
    data[LaCrosse::FRAME_LENGTH - 1] = LaCrosse::CalculateCRC(data);

    rfm.SendArray(data, LaCrosse::FRAME_LENGTH);

    rfm.EnableReceiver(true);
  }
}

void HandleCommandI(byte *values, byte size){
  // 14,43,20,0i  -> ID 14, Interval 4.3 Seconds, reset NewBatteryFlagafter 20 minutes, 17.241 kbps
  if (size == 4){
    transmitter.SetParameters(values[0],
                              values[1] * 100,
                              true,
                              values[2] * 60000 + millis(),
                              values[3] == 0 ? 17241ul : 9579ul);
    transmitter.Enable(true);
  }
  else if (size == 1 && values[0] == 0){
    transmitter.Enable(false);
  }


}

void HandleCommandC(byte *values, byte size){
  // 2,1,9,44c    -> Temperatur  21,9ï¿½C and 44% humidity
  // 129,4,5,77c  -> Temperatur -14,5ï¿½C and 77% humidity
  // To set a negative temperature set bit 7 in the first byte (add 128)
  if (size == 4){
    float temperature = (values[0] & 0b0111111) * 10 + values[1] + values[2] * 0.1;
    if (values[0] & 0b10000000) {
      temperature *= -1;
    }

    transmitter.SetValues(temperature, values[3]);
  }
}


// This function is for testing
void HandleCommandX(byte value) {
  LaCrosse::Frame frame;
  frame.ID = 20;
  frame.NewBatteryFlag = true;
  frame.Bit12 = false;
  frame.Temperature = value;
  frame.WeakBatteryFlag = false;
  frame.Humidity = value;

  if (DEBUG) {
    Serial.print("TX: T=");
    Serial.print(frame.Temperature);
    Serial.print(" H=");
    Serial.print(frame.Humidity);
    Serial.print(" NB=");
    Serial.print(frame.NewBatteryFlag);
    Serial.println();
  }

  byte bytes[LaCrosse::FRAME_LENGTH];
  LaCrosse::EncodeFrame(&frame, bytes);
  rfm.SendArray(bytes, LaCrosse::FRAME_LENGTH);

  rfm.EnableReceiver(RECEIVER_ENABLED);
}

void HandleCommandV() {
  Serial.print("\n[");
  Serial.print(PROGNAME);
  Serial.print('.');
  Serial.print(PROGVERS);

  Serial.print(" (");
  Serial.print(rfm.GetRadioName());
  Serial.print(")");

  Serial.print(" @");
  if (TOGGLE_DATA_RATE == 30) {
    Serial.print("AutoToggleWH1080 ");
    Serial.print(TOGGLE_DATA_RATE);
    Serial.print(" Seconds");
  }
  else if (TOGGLE_DATA_RATE) {
    Serial.print("AutoToggle ");
    Serial.print(TOGGLE_DATA_RATE);
    Serial.print(" Seconds");
  }
  else {
    Serial.print(DATA_RATE);
    Serial.print(" kbps");
  }

  Serial.print(" / ");
  Serial.print(rfm.GetFrequency());
  Serial.print(" kHz");

  Serial.println(']');
}

// **********************************************************************
void loop(void) {
  // Handle the commands from the serial port
  // ----------------------------------------
  if (Serial.available()) {
    HandleSerialPort(Serial.read());
  }

  // Handle the data rate
  // --------------------
  if (TOGGLE_DATA_RATE > 0) {
    // After about 50 days millis() will overflow to zero
    if (millis() < lastToggle) {
      lastToggle = 0;
    }
    if (millis() > lastToggle + TOGGLE_DATA_RATE * 1000) {
		if ((TOGGLE_DATA_RATE == 30) && (DATA_RATE == 17241ul) && (millis() > (lastWh1080 + 5 * TOGGLE_DATA_RATE * 1000))) {
			// WH1080 48 seconds interval so try another 30 seconds
			lastWh1080 = millis();
			Serial.println("Skip toggle for WH1080");
			HandleCommandV();
		}
		else {
		  if (DATA_RATE == 9579ul) {
			DATA_RATE = 17241ul;
		  }
		  else {
			DATA_RATE = 9579ul;
		  }

		  rfm.SetDataRate(DATA_RATE);
	  }
      lastToggle = millis();
    }
  }

  // Priodically transmit
  // --------------------
  if (transmitter.Transmit()) {
    jeeLink.Blink(2);
    rfm.EnableReceiver(RECEIVER_ENABLED);
  }

  // Handle the data reception
  // -------------------------
  if (RECEIVER_ENABLED) {
      byte payload[PAYLOADSIZE];
      byte payLoadSize;
      byte packetCount;
      if (rfm.ReceiveGetPayloadWhenReady(payload, payLoadSize, packetCount)) {
		byte startNibble = (payload[0] & 0xF0)>>4;
      if(ANALYZE_FRAMES) {
        LaCrosse::AnalyzeFrame(payload, fOnlyIfValid);
        LevelSenderLib::AnalyzeFrame(payload, fOnlyIfValid);
        EMT7110::AnalyzeFrame(payload, fOnlyIfValid);
        TX38IT::AnalyzeFrame(payload, fOnlyIfValid);
        switch(startNibble) {
        case 0x5: // WS3000 weather
        case 0x6: // WS3000 time
        case 0xA: //WS4000 WH1080 weather
			if (packetCount <= 1) {
				WS1600::AnalyzeFrame(payload, fOnlyIfValid);
				break;
			}
        case 0xB: //WS4000 WH1080 time
			if (packetCount > 1) {
				WH1080::AnalyzeFrame(payload, packetCount, fOnlyIfValid);
			}
			break;
		}
        Serial.println();
      }
      else {
        jeeLink.Blink(1);

        if (DEBUG) {
          Serial.print("\nEnd receiving, HEX raw data: ");
          for (int i = 0; i < 16; i++) {
            Serial.print(payload[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        }

        byte frameLength = 0;

        // Try LaCrosse like TX29DTH
        if (LaCrosse::TryHandleData(payload, fFhemDisplay)) {
          frameLength = LaCrosse::FRAME_LENGTH;
        }

        // Try LevelSender
        else if (LevelSenderLib::TryHandleData(payload, fFhemDisplay)) {
          frameLength = LevelSenderLib::FRAME_LENGTH;
        }

        // Try EMT7110
        else if (EMT7110::TryHandleData(payload, fFhemDisplay)) {
          frameLength = EMT7110::FRAME_LENGTH;
        }

        // Try WT440XH
        else if (WT440XH::TryHandleData(payload, fFhemDisplay)) {
          frameLength = WT440XH::FRAME_LENGTH;
        }

        // Try TX38IT
        else if (TX38IT::TryHandleData(payload, fFhemDisplay)) {
          frameLength = TX38IT::FRAME_LENGTH;
        }
        else if (packetCount > 1) { // Try WH1080 with frameLength 9 or 10
			switch(startNibble) {
			case 0x5: // WS3000 weather
			case 0x6: // WS3000 time
			case 0xA: //WS4000 WH1080 weather
			case 0xB: //WS4000 WH1080 time
				frameLength = WH1080::TryHandleData(payload, packetCount, fFhemDisplay);
				if (frameLength > 0) {
					lastWh1080 = millis();
					if (TOGGLE_DATA_RATE == 30) { // WH1080 48 seconds interval so switch now
						lastToggle = lastWh1080 - TOGGLE_DATA_RATE * 1000;
					}
				}
				break;
			}
			//frameLength = 0;
		}
		else if (startNibble = 0xA) { // try ws1600 with variable framelength
#if 0
				static unsigned long lastMillis;
				byte datasets = payload[1] & 0x0F; // 2bytes, 4 nibbles
				if (datasets <= 5) { // odd, blank lower 0 of last byte
					byte pktlen = datasets * 2 + 2 + 1;
					if (SensorBase::CalculateCRC(payload, pktlen) == 0) {
						payLoadSize = pktlen;
					}
					SensorBase::DisplayFrame(lastMillis, "ws1600", payLoadSize == pktlen, payload, (payLoadSize > 16) ? 16 : payLoadSize);
				}
	            Serial.println();
#endif
				frameLength = WS1600::TryHandleData(payload, fFhemDisplay);
	            //Serial.print(" frameLength");
	            //Serial.print(frameLength);
	            //Serial.println();
		}

		if (frameLength == 0) {
			// MilliSeconds and the raw data bytes
			static unsigned long lastMillis;
			SensorBase::DisplayFrame(lastMillis, "Unknown", false, payload, (payLoadSize > 16) ? 16 : payLoadSize);

			Serial.print(" Size:");
			Serial.print(payLoadSize);
            Serial.print(" #:");
			Serial.print(packetCount);
            //Serial.print(": ");
			for (byte i = 8; i < payLoadSize; i++) { // test if crc with itself is 0
				if (SensorBase::CalculateCRC(payload, i) == 0) {
					Serial.print(" crclen ");
					Serial.print(i);
					Serial.print(":");
				}
			}

          Serial.println();
		}


        if (RELAY && frameLength > 0) {
          delay(64);
          rfm.SendArray(payload, frameLength);
          if (DEBUG) { Serial.println("Relayed"); }
        }
      }
      rfm.EnableReceiver(true);
    }
  }
}


void setup(void) {
  Serial.begin(57600);
  delay(200);
  if (DEBUG) {
    Serial.println("*** LaCrosse weather station wireless receiver for IT+ sensors ***");
  }

  SetDebugMode(DEBUG);
  LaCrosse::USE_OLD_ID_CALCULATION = USE_OLD_IDS;


  jeeLink.EnableLED(ENABLE_ACTIVITY_LED);
  lastToggle = millis();

  rfm.InitialzeLaCrosse();
  rfm.SetFrequency(INITIAL_FREQ);
  rfm.SetDataRate(DATA_RATE);
  transmitter.Enable(false);
  rfm.EnableReceiver(true);

  if (DEBUG) {
    Serial.println("Radio setup complete. Starting to receive messages");
  }

  // FHEM needs this information
  HandleCommandV();

}
