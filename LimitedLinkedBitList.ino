#include "LimitedLinkedBitList.h"

LimitedLinkedBitList::LimitedLinkedBitList() {
  startIterator = false;
  clear();
}

void LimitedLinkedBitList::add(boolean value) {

  if (size > 0)
    data <<= 1;
  bitWrite(data, 0, value);
  if (size < limit)
    size++;
}

void LimitedLinkedBitList::clear() {
  data = 0b00000000;
  size = 0;
}

void LimitedLinkedBitList::iterate() {
  startIterator = true;
  cursor = 0;
}

boolean LimitedLinkedBitList::hasNext() {
  return cursor < size;
}

boolean LimitedLinkedBitList::next() {
  if (cursor < limit)
    return bitRead(data, cursor++);
  else
    return false;
}

byte LimitedLinkedBitList::getSize() {
  return size;
}

byte LimitedLinkedBitList::getLimit() {
  return limit;
}
