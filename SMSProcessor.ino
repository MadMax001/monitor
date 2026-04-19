#include "SMSProcessor.h"

volatile unsigned long SMSProcessor::watchTimer;
byte SMSProcessor::netStatusInt;
byte SMSProcessor::netStatusPin;
volatile int SMSProcessor::netStatusPahseDuration[2];

SMSProcessor::SMSProcessor(SoftwareSerial *SIM900, byte _netStatusPin, byte _restartPin, CallbackFunction _callbackAfterSuccessInitialization, CallbackFunction _callbackAfterError) {
  atProcessor = new GSMCommandProcessor(SIM900);
  commandIndex = 0;
  messageIndex = 1;
  attempts = 0;
  flags = 0b0000000000000000;
  mode = NONE;
  
  callbackAfterSuccessInitialization = _callbackAfterSuccessInitialization;
  callbackAfterError = _callbackAfterError;
  
  netStatusPin = _netStatusPin;
  pinMode(netStatusPin, INPUT);
  switch(netStatusPin) {                          //Nano, Uno, Mini, Mega
    case 2: netStatusInt = 0; break;
    case 3: netStatusInt = 1; break;
    default: netStatusInt = -1;
  }
  restartPin = _restartPin;
  pinMode(restartPin, OUTPUT);
  digitalWrite(restartPin, HIGH);
}

boolean SMSProcessor::isBusy() {
  return bitRead(flags, BUSY_FLAG);
}

boolean SMSProcessor::isError() {
  return bitRead(flags, ERROR_FLAG);
}

boolean SMSProcessor::isReady() {
  return !(bitRead(flags, BUSY_FLAG) || bitRead(flags, ERROR_FLAG) || bitRead(flags, CHECK_MESSAGES_FLAG));
}

void SMSProcessor::init() {
  if (!isBusy() && !isError()) {
    blockBegin(INIT);
    bitClear(flags, INITIALIZATION_COMPLETE_FLAG);
  }
}

boolean SMSProcessor::isInitializationComplete() {
  return !bitRead(flags, ERROR_FLAG) && /*!bitRead(flags, BUSY_FLAG) &&*/ bitRead(flags, INITIALIZATION_COMPLETE_FLAG);
}

void SMSProcessor::sendMessage(char *message) {
  if (!isError()) {
      if (bitRead(flags, INITIALIZATION_COMPLETE_FLAG)) {
        blockBegin(SEND, SENDING_FLAG);
        messageBuf = message;

        #ifdef DEBUG_SMSProcessor
        Serial.print(F("Send message: "));  Serial.println(message);
        #endif
      } else {
        blockFinish(SENDING_FLAG, true);
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("Can't send message bofore initialization"));
        #endif
      }
  } else {
    #ifdef DEBUG_SMSProcessor
    Serial.println(F("Error! Can't send a message"));
    #endif
  }
}

void SMSProcessor::blockFinish(byte bitNumber, boolean _error) {
  bitClear(flags, BUSY_FLAG);
  bitWrite(flags, ERROR_FLAG, _error);
  if (bitNumber >= 0)
    bitClear(flags, bitNumber);
  mode = NONE;
}

void SMSProcessor::blockBegin(ProcessMode _mode, byte bitNumber) {
  bitSet(flags, BUSY_FLAG);
  bitClear(flags, ERROR_FLAG);
  if (bitNumber >= 0)
    bitSet(flags, bitNumber);
  mode = _mode;
  
  commandIndex = 0;
  //memset(commandBuf, 0, sizeof(commandBuf));
}

void SMSProcessor::unsuccessfulAttempt(char *answer, byte bitNumber) {
        attempts++;

        #ifdef DEBUG_SMSProcessor
        Serial.print(F("attempts: ")); Serial.println(attempts);
        #endif
        
        if (attempts >= MAX_ATTEMPTS_FOR_COMMAND_REPEAT) {
      
          blockFinish(bitNumber, true);
      
          #ifdef DEBUG_SMSProcessor
          Serial.println(F("Fatal error!"));
          #endif

          if (callbackAfterError != NULL) {
            callbackAfterError();
          }
        }

        #ifdef DEBUG_SMSProcessor
        Serial.print(F("error (")); Serial.print(answer); Serial.println(F("), another attempt"));;
        #endif
}

void SMSProcessor::step() {
  if (bitRead(flags, BUSY_FLAG)) {
    if (atProcessor->canSendCommand() && !bitRead(flags, RECEIVING_ANSWER_FLAG)) {
      memset(commandBuf, 0, sizeof(commandBuf));
      switch (mode) {
        case INIT:    initCommandsSendingStep(); break;
        case SEND:    sendCommandsSendingStep(); break;
        case READ:    readCommandsSendingStep(); break;
        case CHECK:   checkCommandsSendingStep(); break;
        case BALANCE: balanceCommandsSendingStep(); break;
        case POWEROFF: powerOffCommandSendingStep(); break;
      }
    }

    if (atProcessor->canGetAnswer() && bitRead(flags, RECEIVING_ANSWER_FLAG)) {
      switch (mode) {
        case INIT:    initCommandsAnswerReceivingStep(atProcessor->getAnswer()); break;
        case SEND:    sendCommandsAnswerReceivingStep(atProcessor->getAnswer()); break;
        case READ:    readCommandsAnswerReceivingStep(atProcessor->getAnswer()); break;
        case BALANCE: balanceCommandsAnswerReceivingStep(atProcessor->getAnswer()); break;
        case POWEROFF: atProcessor->getAnswer(); powerOffCommandsAnswerReceivenigStep(); break;
      }     
      bitClear(flags, RECEIVING_ANSWER_FLAG);
    }

    if (mode == POWERON && bitRead(flags, POWER_OFF_COMPLETE_FLAG) && !bitRead(flags, POWER_ON_COMPLETE_FLAG)) {
      switch (commandIndex) {
        case 0: powerOnPWRKeyDown(); break;
        case 1: if (millis() - timer > POWER_KEY_DOWN_DURATION)
                  powerOnPWRKeyUp();
                break;
      }
    }
  }
  atProcessor->step();
}

void SMSProcessor::initCommandsSendingStep() {
        if (commandIndex < sizeof(initCommands)/sizeof(initCommands[0])) {
          
          strcpy_P(commandBuf, (char*)pgm_read_word(&(initCommands[commandIndex])));
        
          #ifdef DEBUG_SMSProcessor
          Serial.print(F("command: ")); Serial.println(commandBuf);
          #endif
        
          atProcessor->sendATcommand(commandBuf);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
        } else {
          if (!isError()) {
            bitSet(flags, INITIALIZATION_COMPLETE_FLAG);
            blockFinish();
//            checkMessageTimer = millis();
          
            #ifdef DEBUG_SMSProcessor
            Serial.println(F("Initialization is done!!"));
            #endif

            if (callbackAfterSuccessInitialization != NULL)
              callbackAfterSuccessInitialization();
          }
        }
}

void SMSProcessor::initCommandsAnswerReceivingStep(char *answer) {
  if (commandIndex == 0  || commandIndex > 0 && commandIndex < sizeof(initCommands)/sizeof(initCommands[0]) - 1 && strcmp_P(answer, OKAnswer) == 0/* && strstr_P(answer, OKAnswer)*/) {
      commandIndex++;
      attempts = 0;
  } else if (commandIndex == sizeof(initCommands)/sizeof(initCommands[0]) - 1 && strstr_P(answer, RegAnswerPrefix)/* && strstr_P(answer, OKAnswer)*/) {
    
    if (checkForNetRegistration(answer)) {
      commandIndex++;
      attempts = 0;
    } else {
      blockFinish(EMPTY_FLAG, true);

      #ifdef DEBUG_SMSProcessor
      Serial.println(F("No network registration"));
      #endif

      if (callbackAfterError != NULL) {
        callbackAfterError();
      }
    }
    
  }else {
    unsuccessfulAttempt(answer);
  }
}

void SMSProcessor::sendCommandsSendingStep() {
  switch(commandIndex) {
    case 0:
          for (int i = 0; i < strlen_P(commandInit2); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandInit2[i]));

          #ifdef DEBUG_SMSProcessor
          Serial.print(F("wakeup command: ")); Serial.println(commandBuf);
          #endif
        
          atProcessor->sendATcommand(commandBuf);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
          break;
    case 1:
          int i;
          for (i = 0; i < strlen_P(commandMessageSend); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandMessageSend[i]));
          for (int j = 0; j < strlen_P(phoneNumber); j++)
            commandBuf[i+j] = ((char)pgm_read_byte(&phoneNumber[j]));

          #ifdef DEBUG_SMSProcessor
          Serial.print(F("command: ")); Serial.println(commandBuf);
          #endif
        
          atProcessor->sendATcommand(commandBuf);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
          break;
    case 2:
          #ifdef DEBUG_SMSProcessor
          Serial.print(F("message: ")); Serial.println(messageBuf);
          #endif
        
          String command = String(messageBuf) + char(26);                                                                   
          atProcessor->sendATcommand(command.c_str(), 5000);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
  }
}

void SMSProcessor::sendCommandsAnswerReceivingStep(char* answer) {
  switch(commandIndex) {
    case 0:
      commandIndex++;
      break;
    case 1:
      if (strcmp_P(answer, SMSSendAnswer) == 0) {
        commandIndex++;
        attempts = 0;
      } else {
        unsuccessfulAttempt(answer, SENDING_FLAG);
      }
      break;
    case 2:
      //if (String(answer).indexOf(String(OKAnswer)) > 0) {
      if (strstr_P(answer, OKAnswer)) {
        commandIndex++;
        attempts = 0;        

        blockFinish(SENDING_FLAG);          
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("Sending is done!!"));
        #endif
        if (bitRead(flags, CHECK_MESSAGES_FLAG)) {
          blockBegin(CHECK, CHECK_MESSAGES_FLAG);   
        }
       
      } else {
        unsuccessfulAttempt(answer, SENDING_FLAG);
      }
  }
}


boolean SMSProcessor::checkForNetRegistration(char *answer) {
   return strstr_P(ProjectUtils::subStr(answer, ",", 2), (const char*) F("1"));
}

void SMSProcessor::getMessage(char *cellNumber) {
  if (!isBusy() && !isError()) {
      blockBegin(READ, READING_FLAG);
      strcpy(messageNum, cellNumber);

      #ifdef DEBUG_SMSProcessor
      Serial.print(F("Read message from cell: ")); Serial.println(messageNum);
      #endif
  } else {
    #ifdef DEBUG_SMSProcessor
    Serial.println(F("Busy or Error!"));
    #endif
  }
}

void SMSProcessor::readCommandsSendingStep() {
  switch(commandIndex) {
    case 0:
          for (int i = 0; i < strlen_P(commandInit2); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandInit2[i]));

          #ifdef DEBUG_SMSProcessor
          Serial.print(F("wakeup command: ")); Serial.println(commandBuf);
          #endif
        
          atProcessor->sendATcommand(commandBuf);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
          break;
    case 1:
          for (int i = 0; i < strlen_P(commandMessageRead); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandMessageRead[i]));
          strcat(commandBuf, messageNum);
          #ifdef DEBUG_SMSProcessor
          Serial.print(F("command: ")); Serial.println(commandBuf);
          #endif
        
          atProcessor->sendATcommand(commandBuf);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
          break;
    case 2:
          for (int i = 0; i < strlen_P(commandMessageDelete); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandMessageDelete[i]));
          strcat(commandBuf, messageNum);
          
          #ifdef DEBUG_SMSProcessor
          Serial.print(F("command: ")); Serial.println(commandBuf);
          #endif
        
          atProcessor->sendATcommand(commandBuf);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
          break;
  }
    
}

void SMSProcessor::readCommandsAnswerReceivingStep(char *answer) {
  switch(commandIndex) {
    case 0:
      commandIndex++;
      break;
    case 1:
      if (strcmp_P(answer, ERRORAnswer) == 0 && bitRead(flags, CHECK_MESSAGES_FLAG)) {
         blockFinish(READING_FLAG);  
         messageIndex = 1;
         bitClear(flags, CHECK_MESSAGES_FLAG);
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("Check for the messages is over"));
        #endif
      } else if (strcmp_P(answer, OKAnswer) == 0) {
        commandIndex++;
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("Empty message"));
        #endif
      } else if (strlen(answer) <= UTILS_BUF_SIZE) {
        char *number = ProjectUtils::subStr(answer, ",", 2);
        if (strcmp_P(number, phoneNumber) == 0) {
          if (!isNewMessage()) {                                                                                      //если нет непрочитанного готового сообщения
            newMessageBuf = ProjectUtils::subStr(ProjectUtils::subStr(answer, ",", 5), "\"", 2);
            int sourceLen = strlen(newMessageBuf);
            int suffixOKLen = strlen_P(OKAnswer);
            if (ProjectUtils::endsWith_P(newMessageBuf, OKAnswer) && sourceLen > suffixOKLen) {
              bitSet(flags, NEW_MESSAGE_EXISTS_FLAG);
              newMessageBuf[sourceLen - suffixOKLen] = char(0);
              #ifdef DEBUG_SMSProcessor
              Serial.println(F("New message"));
              #endif
            }
          } else {
            blockFinish(READING_FLAG);          
            #ifdef DEBUG_SMSProcessor
            Serial.println(F("The unread message already exists"));
            #endif
          }
          commandIndex++;
      
        } else {
          blockFinish(READING_FLAG);  
          #ifdef DEBUG_SMSProcessor
          Serial.print(F("Wrong phone number: ")); Serial.print(number);
          #endif
        }
      } else {
        blockFinish(READING_FLAG, true);  
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("The length of the answer phrase exceeds the buffer size"));
        #endif
      }
      break;
    case 2:
      if (strstr_P(answer, OKAnswer)) {
        commandIndex++;
        attempts = 0;        

        blockFinish(READING_FLAG);          
        if (bitRead(flags, NEW_MESSAGE_EXISTS_FLAG)) {
          bitSet(flags, NEW_MESSAGE_FLAG);
          bitClear(flags, NEW_MESSAGE_EXISTS_FLAG);
        }
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("Reading is done!!"));
        #endif
        if (bitRead(flags, CHECK_MESSAGES_FLAG)) {
          blockBegin(CHECK);
        }
      } else {
        unsuccessfulAttempt(answer, READING_FLAG);
      }
  }
}

boolean SMSProcessor::isNewMessage() {
  return bitRead(flags, NEW_MESSAGE_FLAG);
}

char* SMSProcessor::readMessage() {
  bitClear(flags, NEW_MESSAGE_FLAG);
  return newMessageBuf;
}

void SMSProcessor::checkCommandsSendingStep() {
  #ifdef DEBUG_SMSProcessor
  Serial.println(F("Check message: "));  Serial.println(String(messageIndex));
  #endif
  blockFinish();
  getMessage(String(messageIndex++).c_str());

}

void SMSProcessor::checkBalance() {
  if (!isError() && (!isBusy() && !bitRead(flags, CHECK_MESSAGES_FLAG) || bitRead(flags, CHECK_MESSAGES_FLAG))) {
    if (bitRead(flags, INITIALIZATION_COMPLETE_FLAG)) {
      blockBegin(BALANCE);

      #ifdef DEBUG_SMSProcessor
      Serial.println(F("Check balance"));
      #endif
    } else {
      blockFinish(EMPTY_FLAG, true);
      #ifdef DEBUG_SMSProcessor
      Serial.println(F("Can't check balance bofore initialization"));
      #endif
    }
  } else {
    #ifdef DEBUG_SMSProcessor
    Serial.println(F("Busy or Error!"));
    #endif
  }
}

void SMSProcessor::balanceCommandsSendingStep() {
  switch(commandIndex) {
    case 0:
          for (int i = 0; i < strlen_P(commandInit2); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandInit2[i]));

          #ifdef DEBUG_SMSProcessor
          Serial.print(F("wakeup command: ")); Serial.println(commandBuf);
          #endif
        
          atProcessor->sendATcommand(commandBuf);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
          break;
    case 1:
          for (int i = 0; i < strlen_P(commandCheckBalance); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandCheckBalance[i]));
        
          atProcessor->sendATcommand(commandBuf, 5000);
          bitSet(flags, RECEIVING_ANSWER_FLAG);
    
  }
}

void SMSProcessor::balanceCommandsAnswerReceivingStep(char *answer) {
  switch(commandIndex) {
    case 0:
      commandIndex++;
      break;
    case 1:
      if (strstr_P(answer, OKAnswer)) {
        commandIndex++;
        attempts = 0;        

        blockFinish(SENDING_FLAG);          
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("Sending is done!!"));
        #endif

        blockFinish();
        bitSet(flags, NEW_MESSAGE_FLAG);
        newMessageBuf = ProjectUtils::subStr(answer, "\"", 2);
            
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("Balance info is ready!"));
        #endif
        if (bitRead(flags, CHECK_MESSAGES_FLAG)) {
          blockBegin(CHECK);
        }

      } else {
        unsuccessfulAttempt(answer, SENDING_FLAG);
      }
  }
}

void SMSProcessor::checkMessages() {
  blockBegin(CHECK, CHECK_MESSAGES_FLAG);  
}

void SMSProcessor::startNetStatusWatching() {
  netStatusPahseDuration[0] = 0;
  netStatusPahseDuration[1] = 0;
  attachInterrupt(netStatusInt, changeNetStatusPhase, CHANGE);
}

void SMSProcessor::stopNetStatusWatching() {
  detachInterrupt(netStatusInt);  
}

static void SMSProcessor::changeNetStatusPhase() {
  netStatusPahseDuration[digitalRead(netStatusPin) ? LED_IS_ON : LED_IS_OFF] = millis() - watchTimer;
  watchTimer = millis();
}

SMSProcessor::NetStatus SMSProcessor::getNetStatus() {
  if (netStatusPahseDuration[0] > 0 && netStatusPahseDuration[0] < 100) {
    if (netStatusPahseDuration[1] > 2000) return REGISTERED;
    if (netStatusPahseDuration[1] > 700 && netStatusPahseDuration[1] < 1000) return NOT_REGISTERED;
    if (netStatusPahseDuration[1] > 200 && netStatusPahseDuration[1] < 400) return GPRS;
  }
  return UNDEFINED;
}

boolean SMSProcessor::isNetRegistered() {
  return (getNetStatus() == REGISTERED);
}

void SMSProcessor::restart() {
  #ifdef DEBUG_SMSProcessor
  Serial.println(F("Powering down"));
  #endif
  blockBegin(POWEROFF);
  bitClear(flags, POWER_OFF_COMPLETE_FLAG);  
  bitClear(flags, POWER_ON_COMPLETE_FLAG);
}

void SMSProcessor::powerOffCommandSendingStep() {
  switch(commandIndex) {
    case 0:
          for (int i = 0; i < strlen_P(commandInit2); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandInit2[i]));

          #ifdef DEBUG_SMSProcessor
          Serial.print(F("wakeup command: ")); Serial.println(commandBuf);
          #endif
        
          atProcessor->sendATcommand(commandBuf);
          break;
    case 1:
          for (int i = 0; i < strlen_P(commandPowerOff); i++)
            commandBuf[i] = ((char)pgm_read_byte(&commandPowerOff[i]));
        
          #ifdef DEBUG_SMSProcessor
          Serial.print(F("command: ")); Serial.println(commandBuf);
          #endif

          atProcessor->sendATcommand(commandBuf, 5000);                                 //послали команду на выключение и ждем на всякий случай 5 сек
  }
  bitSet(flags, RECEIVING_ANSWER_FLAG);
}

void SMSProcessor::powerOffCommandsAnswerReceivenigStep() {
  switch(commandIndex) {
    case 0:
        commandIndex++;
        break;  
    case 1:
        blockFinish();          
        #ifdef DEBUG_SMSProcessor
        Serial.println(F("Power is OFF"));
        #endif
        bitSet(flags, POWER_OFF_COMPLETE_FLAG);
        blockBegin(POWERON);
        bitClear(flags, POWER_ON_COMPLETE_FLAG);  
  }
  
}

void SMSProcessor::powerOnPWRKeyDown() {
  #ifdef DEBUG_SMSProcessor  
  Serial.println(F("PWRKey down"));
  #endif
  digitalWrite(restartPin, LOW);
  timer = millis();
  commandIndex++;  
}

void SMSProcessor::powerOnPWRKeyUp() {
  #ifdef DEBUG_SMSProcessor  
  Serial.println(F("PWRKey up"));
  #endif
  digitalWrite(restartPin, HIGH);
  blockFinish();
  flags = 0b0000000000000000;
  bitSet(flags, POWER_ON_COMPLETE_FLAG);
}

boolean SMSProcessor::isRestarting() {
  return !isBusy() && bitRead(flags, POWER_ON_COMPLETE_FLAG);
}
