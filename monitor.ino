#include "monitor.h"

void setup() {
  
  #ifdef DEBUG_Monitor
  Serial.begin(9600);
  while(!Serial);
  Serial.println(F("Pause before initialization"));
  delay(100);
  #endif

  softSetup();
  hardSetup();
  
}

void softSetup() {
  flags = 0b00000000;
  index = 0;

  sms.startNetStatusWatching();
  timer = millis();
}

void hardSetup() {
  SIM900.begin(9600); 

  calibrate(8300);    
  /*#ifdef DEBUG_Monitor
  int maxTimeOut = getMaxTimeout();
  Serial.println(String(maxTimeOut));
  #endif*/

  #ifdef Voltage_Monitoring
  pinMode(BATTERY_LEVEL_PIN, INPUT);
  #endif
  pinMode(HUMIDITY_BATHROOM_PIN, INPUT);
  pinMode(HUMIDITY_KITCHEN_PIN, INPUT);
  pinMode(HUMIDITY_ROOM_PIN, INPUT);

  for (byte i = 0; i <= A5; i++) {                //для энергосбережения
    bitClear(flags, FIND_IN_ARRAY_FLAG);
    for (byte j = 0; j < sizeof(pinsInUse); j++) {
      if ((byte)pgm_read_byte(&pinsInUse[j]) == i) {
        bitSet(flags, FIND_IN_ARRAY_FLAG);
        break;
      }
    }
    if (!bitRead(flags, FIND_IN_ARRAY_FLAG)) {
      pinMode (i, OUTPUT);    
      digitalWrite (i, LOW);  
    }
  }
  bodInSleep(false);
}

void loop() {
  if (!bitRead(flags, STOP_FLAG) && !bitRead(flags, START_PAUSE_COMPLETE_FLAG) && !bitRead(flags, RESTART_MODEM_FLAG)) {                                                                         //начальная пауза для инициализации модема
    #ifdef DEBUG_Monitor
    delay(1000);
    Serial.println(F("Wait for the startup"));
    #endif
    if (sms.isNetRegistered()) {
      bitSet(flags, START_PAUSE_COMPLETE_FLAG);
      sms.stopNetStatusWatching();
      sms.init();
    } else if (millis() - timer > START_PAUSE_DURATION) {
        SIM900Error();
        bitSet(flags, STOP_FLAG);
    }
  }
  
  if (sms.isInitializationComplete() && !bitRead(flags, RESTART_MODEM_FLAG)) {
    if (!bitRead(flags, SUCCESS_BEEP_COMPLETE) && millis() - timer > SUCCESS_BEEPER_PIN_DURATION) {
      bitSet(flags, SUCCESS_BEEP_COMPLETE);
    }
    if (sms.isReady() && bitRead(flags, SUCCESS_BEEP_COMPLETE)) {
        #ifdef DEBUG_Monitor
        ProjectUtils::printDebugStatus(flags);
//        Serial.print(F("Voltage: "));
//        Serial.println(String(batteryLevel));
        delay(8192);
        #else
        sleep(SLEEP_8192MS);
        #endif
        
        if (!bitRead(flags, STOP_FLAG) && !bitRead(flags, ALARM_FLAG)) {
          readSensorsAction();
        }

        if (index > CHECK_MESSAGE_LIST_IN_SENSOR_PERIOD_CYCLES) {
          checkAction();
        } else {
            index++;
        }
    }

    if (sms.isNewMessage()) {
      newMessageAction();
    }
  }

  if (bitRead(flags, RESTART_MODEM_FLAG) && sms.isRestarting()) {
    afterRestartAction();
  }

  if (sms.isError() && !bitRead(flags, RESTART_MODEM_FLAG)) {
    errorAction();
  }

  sms.step();
}


void readSensorsAction() {
    for (int i = 0; i < sizeof(sensors)/sizeof(sensors[0]); i++) {
      sensors[i].values->add(!digitalRead(sensors[i].pin));
      if (checkForAlarm(&sensors[i])) {
          bitSet(flags, ALARM_FLAG);
          memset(sendingMessageBuf, 0, sizeof(sendingMessageBuf));
          strcat_P(sendingMessageBuf, alarm);
          strcat(sendingMessageBuf, sensors[i].name);
          sms.sendMessage(sendingMessageBuf);
      }
  }
}

void checkAction() {
#ifdef Voltage_Monitoring
  if (!bitRead(flags, BATTERY_FLAG) && checkBatteryLevel()) {
      #ifdef DEBUG_Monitor
      Serial.println(F("Low level!"));
      #endif
      batteryLowLevelAlarm();
  } else {
      sms.checkMessages();
      index = 0;
  }  
#else
      sms.checkMessages();
      index = 0;
#endif
}

void newMessageAction() {
        #ifdef DEBUG_Monitor
        Serial.print(F("Message: ")); Serial.println(sms.readMessage());
        #endif
        
        if (!bitRead(flags, BALANCE_MESSAGE_FLAG)) {
          switch (parseMessage(sms.readMessage())) {
            case STOP_MONITORING: stopMonitoring(); break;
            case GET_STATUS: sendStatus(); break;
            case CHECK_BALANCE: checkBalance(); break;
            case RESUME_MONITORING: resumeMonitoring(); break;
          }
        } else {
          sms.sendMessage(sms.readMessage());
          bitClear(flags, BALANCE_MESSAGE_FLAG);
        }
}

void errorAction() {
    bitSet(flags, STOP_FLAG);
    #ifndef DEBUG_Monitor
    sleep(SLEEP_FOREVER);
    #endif
}

void afterRestartAction() {
    #ifdef DEBUG_Monitor
    Serial.println(F("Restarting is complete"));
    #endif
    softSetup();
}

void stopMonitoring() {
  #ifdef DEBUG_Monitor
  Serial.println(F("Stop"));
  #endif
  bitSet(flags, STOP_FLAG);
}

void resumeMonitoring() {
  #ifdef DEBUG_Monitor
  Serial.println(F("Resume"));
  #endif
  bitClear(flags, STOP_FLAG);
  bitClear(flags, ALARM_FLAG);
  bitClear(flags, BATTERY_FLAG);
  for (int i = 0; i < sizeof(sensors)/sizeof(sensors[0]); i++) {
    sensors[i].values->clear();
  }
}

void checkBalance() {
  sms.checkBalance(); 
  bitSet(flags, BALANCE_MESSAGE_FLAG);  
}

Request parseMessage(char *message) {
  if (strcasecmp_P(message, stopMessage) == 0) {
    return STOP_MONITORING;  
  }
  if (strcasecmp_P(message, statusMessage) == 0) {
    return GET_STATUS;  
  }
  if (strcasecmp_P(message, balanceMessage) == 0) {
    return CHECK_BALANCE;  
  }
  if (strcasecmp_P(message, resumeMessage) == 0) {
    return RESUME_MONITORING;  
  }  
  return UNRECOGNIZED;
}

boolean checkForAlarm(SensorMeta *sensor) {
  if (sensor->values->getSize() == sensor->values->getLimit()) {
    sensor->values->iterate();
    while (sensor->values->hasNext()) {
      if (!sensor->values->next()) 
        return false;
    }
    return true;
  }
  return false;
}

void sendStatus() {
    memset(sendingMessageBuf, 0, sizeof(sendingMessageBuf));
    if (bitRead(flags, STOP_FLAG)) {
      strcat_P(sendingMessageBuf, statusIsStoppedMessage);
    } else if (bitRead(flags, ALARM_FLAG)) {
      strcat_P(sendingMessageBuf, statusAlarmMessage);
    } else {
      strcat_P(sendingMessageBuf, statusIsWorkingMessage);
    }
    if (bitRead(flags, ALARM_FLAG)) {
      for (int i = 0; i < sizeof(sensors)/sizeof(sensors[0]); i++) {
         if (checkForAlarm(&sensors[i])) {
            strcat_P(sendingMessageBuf, wordDelim);
            strcat(sendingMessageBuf, sensors[i].name);
//            break;
         }
      }
    }

#ifdef Voltage_Monitoring
    strcat_P(sendingMessageBuf, wordDelim);
    strcat_P(sendingMessageBuf, batteryMessage);
    char batteryLevelStr[4];
    itoa(batteryLevel*10, batteryLevelStr, 10);
    memmove(&batteryLevelStr[2], &batteryLevelStr[1], 2);
    batteryLevelStr[1] = '.';
    strcat(sendingMessageBuf, batteryLevelStr);
#endif    
    sms.sendMessage(sendingMessageBuf);
}

void afterSIM900SuccessInit() {
  #if defined(DEBUG_Monitor) || defined(DEBUG_SMSProcessor) || defined(DEBUG_GSMCommandProcessor)
  delay(100);
  #endif
  tone(BEEPER_PIN, 500, SUCCESS_BEEPER_PIN_DURATION);
  timer = millis();
}

void SIM900Error() {
  tone(BEEPER_PIN, 200,50);
  delay(100);
  tone(BEEPER_PIN, 200,50);
  delay(100);
  
  sms.restart();
  bitSet(flags, RESTART_MODEM_FLAG);
}

#ifdef Voltage_Monitoring
boolean checkBatteryLevel() {
  batteryLevel = map(analogRead(BATTERY_LEVEL_PIN), 0, 1023, 0, 500);
  return batteryLevel < LOW_BATTERY_LEVEL_PIN;
}
#endif

#ifdef Voltage_Monitoring
void batteryLowLevelAlarm() {
  bitSet(flags, BATTERY_FLAG);
  memset(sendingMessageBuf, 0, sizeof(sendingMessageBuf));
  strcat_P(sendingMessageBuf, lowLevelMessage);
  sms.sendMessage(sendingMessageBuf);
}
#endif
