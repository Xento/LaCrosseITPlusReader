#ifndef _LACROSSE_h
#define _LACROSSE_h

#include "Arduino.h"
#include "SensorBase.h"


class LaCrosse : public SensorBase {
public:
  struct Frame {
    byte  Header;
    byte  ID;
    bool  NewBatteryFlag;
    bool  Bit12;
    float Temperature;
    byte  Temperature10;
    byte  Temperature1;
    byte  Temperature01;
    bool  WeakBatteryFlag;
    bool  SType;
    byte  Humidity;
    byte  Humidity10;
    byte  Humidity1;
    byte  Humidity01;
    byte  CRC;
    bool  IsValid;
  };

  static const byte FRAME_LENGTH = 5;
  static bool USE_OLD_ID_CALCULATION;
  static byte CalculateCRC(byte data[]);
  static void EncodeFrame(struct LaCrosse::Frame *frame, byte bytes[5]);
  static void DecodeFrame(byte *bytes, struct LaCrosse::Frame *frame);
  static void AnalyzeFrame(byte *data);
  static bool TryHandleData(byte *data);
  static String GetFhemDataString(byte *data);
  static String GetHMSDataString(struct LaCrosse::Frame *frame);
  static void SetHMSMode(boolean mode);
  static bool m_HMSMode;      //FULB
  static bool IsValidDataRate(unsigned long dataRate);
  

protected:
  static String BuildFhemDataString(struct LaCrosse::Frame *frame);

};

#endif

