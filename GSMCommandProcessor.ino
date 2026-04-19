#include "GSMCommandProcessor.h"

GSMCommandProcessor::GSMCommandProcessor(SoftwareSerial *SIM900) {
  #ifdef DEBUG_GSMCommandProcessor
  Serial.println(F("Init"));      
  #endif

      module = SIM900;
      flags = 0b00000000;
}

void GSMCommandProcessor::makeAnswer() {
  #ifdef DEBUG_GSMCommandProcessor
  Serial.println(F("makeAnswer"));  
  #endif

      bitSet(flags, WAITING_ANSWER_FLAG);
      
      for (int i = 0; i < ANSWER_BUFF_SIZE; i++)
        fromModuleData[i] = 0;
      dataIndex = 0;
      answerTimer = millis();
}

void GSMCommandProcessor::sendATcommand(char *command, unsigned long _timeout) {
  #ifdef DEBUG_GSMCommandProcessor
  Serial.print(F("Send: ")); Serial.println(command);  
  #endif
      lastCommand = command;
      module->println(command);
      timeout = _timeout;
      bitSet(flags, SENDING_COMMAND_FLAG);
      makeAnswer();  
}

void GSMCommandProcessor::listenPort(unsigned long _timeout) {
  #ifdef DEBUG_GSMCommandProcessor
  Serial.println(F("Listen"));  
  #endif

      timeout = _timeout;
      bitSet(flags, SENDING_COMMAND_FLAG);
      lastCommand = "";
      makeAnswer();  
}

boolean GSMCommandProcessor::canGetAnswer() {
  return (bitRead(flags, SENDING_COMMAND_FLAG) && !bitRead(flags, WAITING_ANSWER_FLAG));
}

boolean GSMCommandProcessor::canSendCommand() {
  return (!bitRead(flags, SENDING_COMMAND_FLAG) && !bitRead(flags, WAITING_ANSWER_FLAG));
}


char* GSMCommandProcessor::getAnswer() {
  #ifdef DEBUG_GSMCommandProcessor
  Serial.println(F("Get answer"));    
  #endif
  
  boolean allMatch;
  int matchingPosition = 0;
  int commandLen = strlen(lastCommand);

  if (canGetAnswer()) {

    #ifdef DEBUG_GSMCommandProcessor
    Serial.print(F("answer: ")); Serial.println(fromModuleData);
    Serial.print(F("command: ")); Serial.println(lastCommand);
    #endif
    
    if (commandLen > 0) {
      for (int i = 0; i < ANSWER_BUFF_SIZE; i++) {
        allMatch = true;
        for (int j = 0; j < commandLen; j++) {
          allMatch = allMatch && (fromModuleData[i+j] == lastCommand[j]) && ((i+j) < ANSWER_BUFF_SIZE);
          if (allMatch) {
            if (j == 0)
              matchingPosition = i;  
          } else {
            break;
          }
        }
        if (allMatch && matchingPosition + commandLen < ANSWER_BUFF_SIZE) {
          bitClear(flags, SENDING_COMMAND_FLAG);
          return fromModuleData + matchingPosition + commandLen;          //String(fromModuleData).substring(matchingPosition + commandLen);
        }
      }
    } else {
      bitClear(flags, SENDING_COMMAND_FLAG);
      return fromModuleData;  
    }
  }
  bitClear(flags, SENDING_COMMAND_FLAG);
  return *("");
}

void GSMCommandProcessor::step() {
      if (bitRead(flags, WAITING_ANSWER_FLAG)) {                               //считываение ответа
        if (millis() - answerTimer <= timeout) {
          if (module->available()) { 
            c = module->read();
            if (c > 31)
              fromModuleData[dataIndex++] = c;
          }        
        } else {
          bitClear(flags, WAITING_ANSWER_FLAG);
        }
      }
}        
