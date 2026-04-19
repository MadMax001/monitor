#ifndef LimitedLinkedBitList_H
#define LimitedLinkedBitList_H


class LimitedLinkedBitList {
  private:
    byte data;
    byte cursor;
    byte size;
    const byte limit = 8;
    boolean startIterator;
  public:
    LimitedLinkedBitList();
    void add(boolean _value);
    void clear();
    void iterate();
    boolean hasNext();
    boolean next();
    byte getSize();
    byte getLimit();
};
#endif 
