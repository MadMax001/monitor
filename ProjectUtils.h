#ifndef ProjectUtils_H
#define ProjectUtils_H

#define UTILS_BUF_SIZE 128

class ProjectUtils {
  private:
    static char buf[UTILS_BUF_SIZE]; 
  public:
    #if defined(DEBUG_Monitor) || defined(DEBUG_SMSProcessor) || defined(DEBUG_GSMCommandProcessor)
    static void printDebugStatus(byte flags);
    static void printDebugStatus(int flags);
    #endif
    static char* subStr (char* str, char *delim, int index);
    static boolean endsWith_P(char *source, char *pSuffix);
};

#endif
