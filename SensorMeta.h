#ifndef SensorMeta_H
#define SensorMeta_H

#ifndef LimitedLinkedIntList_H
#include "LimitedLinkedBitList.h"
#endif

class SensorMeta {
 public:
  char *name;
  int threshold;
  byte pin;
  LimitedLinkedBitList *values;
  SensorMeta(char *_name, int _threshold, byte _pin);
};

SensorMeta::SensorMeta(char *_name, int _threshold, byte _pin) {
  name = _name;
  threshold = _threshold;
  pin = _pin;
  values = new LimitedLinkedBitList();
}
#endif
