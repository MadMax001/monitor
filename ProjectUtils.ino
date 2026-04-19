#include "ProjectUtils.h"

char ProjectUtils::buf[];

#if defined(DEBUG_Monitor) || defined(DEBUG_SMSProcessor) || defined(DEBUG_GSMCommandProcessor)                             //используется только для режима отладки
void ProjectUtils::printDebugStatus(byte flags) {
  byte numBits = 8;  // 2^numBits must be big enough to include the number n
  char b;
  char c = ' ';   // delimiter character
  for (byte i = 0; i < numBits; i++) {
    b = (flags & (1 << (numBits - 1 - i))) > 0 ? '1' : '0'; // slightly faster to print chars than ints (saves conversion)
    Serial.print(b);
    if (i < (numBits - 1) && ((numBits-i - 1) % 4 == 0 )) Serial.print(c); // print a separator at every 4 bits
  }
  Serial.println("");
}

void ProjectUtils::printDebugStatus(int flags) {
  byte numBits = 16;  // 2^numBits must be big enough to include the number n
  char b;
  char c = ' ';   // delimiter character
  for (byte i = 0; i < numBits; i++) {
    b = (flags & (1 << (numBits - 1 - i))) > 0 ? '1' : '0'; // slightly faster to print chars than ints (saves conversion)
    Serial.print(b);
    if (i < (numBits - 1) && ((numBits-i - 1) % 4 == 0 )) Serial.print(c); // print a separator at every 4 bits
  }
  Serial.println("");
}
#endif

char* ProjectUtils::subStr (char *str, char *delim, int index) {
  char *act, *sub, *ptr;

  // Since strtok consumes the first arg, make a copy
  strcpy(buf, str);
  for (int i = 1, act = buf; i <= index; i++, act = NULL) {
     sub = strtok_r(act, delim, &ptr);
     if (sub == NULL) break;
  }
  return sub;
}

boolean ProjectUtils::endsWith_P(char *source, char *pSuffix) {
  int sourceLen = strlen(source);
  int suffixLen = strlen_P(pSuffix);

  if (sourceLen < suffixLen)
    return false;
  if (suffixLen == 0)
    return true;
  
  for (int i = suffixLen - 1; i >= 0; i--) {
    if (((char)pgm_read_byte(&pSuffix[i])) != source[sourceLen - suffixLen + i])
      return false;
  }

  return true;
}
