#ifndef monitor_H
#define monitor_H

//#define DEBUG_Monitor
#define Voltage_Monitoring

#define FIX_TIME // Коррекция времени в случае изменения системной частоты
// дефайнить до подключения библиотеки!!!
#include "GyverPower.h"

#include <SoftwareSerial.h>
#include "SMSProcessor.h"
#include "SensorMeta.h"

#ifdef ProjectUtils_H
#include "ProjectUtils.h"
#endif

#define RX_PIN 10 // -> 5VT SIM900
#define TX_PIN 11 // -> 5VR SIM900
#define NETSTATUS_PIN 2             //сетевой статус SIM900
#define RESTART_PIN 7               //управление перезапуском SIM900

#define BEEPER_PIN 9                //зуммер
#define HUMIDITY_BATHROOM_PIN 4     //датчик в ванной
#define HUMIDITY_KITCHEN_PIN 5      //датчик на кухне
#define HUMIDITY_ROOM_PIN 6         //датчик в комнате
#define BATTERY_LEVEL_PIN A0        //уровень аккумулятора

#define START_PAUSE_DURATION 180000                                  //начальный период для запуска модема перед его инициализацией
#define CHECK_MESSAGE_LIST_IN_SENSOR_PERIOD_CYCLES 37               //период проверки сообщений, пришедших в оффлайн режиме, в периодах опроса датчика. Сейчас он 8 сек
#define LOW_BATTERY_LEVEL_PIN 305                                       //малый уровень заряда аккумулятора, при котором отсылается оповещение
#define SUCCESS_BEEPER_PIN_DURATION 500                                 //длительность звукового сигнала в случае успешной инициализации

#define STOP_FLAG 0                           //побитовый флаг, отвечающий за остановку монитора
#define BALANCE_MESSAGE_FLAG 1                //побитовый флаг, отвечающий за переключения вида входящего сообщения - баланс
#define ALARM_FLAG 2                          //побитовый флаг, отвечающий за наступление тревоги
#define START_PAUSE_COMPLETE_FLAG 3           //побитовый флаг, отвечающий за задержку при старте для инициализации модема
#define FIND_IN_ARRAY_FLAG 4                  //побитовый флаг, используемый в поиске свободных пинов для энергосбережения
#define BATTERY_FLAG 5                        //побитовый флаг, сигнализирующий о низком уровне аккумулятора
#define SUCCESS_BEEP_COMPLETE 6               //побитовый флаг, сигнализирующий о завершении звукового сигнала об успешной инициализации
#define RESTART_MODEM_FLAG 7                  //побитовый флаг, сигнализирующий о запуске процедуры перезагрузки модема

#ifdef Voltage_Monitoring
const byte pinsInUse[] PROGMEM = {BEEPER_PIN, HUMIDITY_BATHROOM_PIN, HUMIDITY_KITCHEN_PIN, HUMIDITY_ROOM_PIN, RX_PIN, TX_PIN, NETSTATUS_PIN, RESTART_PIN, BATTERY_LEVEL_PIN, 0, 1};
#else
const byte pinsInUse[] PROGMEM = {BEEPER_PIN, HUMIDITY_BATHROOM_PIN, HUMIDITY_KITCHEN_PIN, HUMIDITY_ROOM_PIN, RX_PIN, TX_PIN, NETSTATUS_PIN, RESTART_PIN, /*BATTERY_LEVEL_PIN,*/ 0, 1};
#endif

const char stopMessage[] PROGMEM = "stop";
const char statusMessage[] PROGMEM = "status";
const char balanceMessage[] PROGMEM = "balance";
const char resumeMessage[] PROGMEM = "resume";

const char alarm[] PROGMEM = "alarm: ";
const char wordDelim[] PROGMEM = "  ";
const char statusIsWorkingMessage[] PROGMEM = "is working";
const char statusIsStoppedMessage[] PROGMEM = "is stopped";
const char statusAlarmMessage[] PROGMEM = "alarm";
#ifdef Voltage_Monitoring
const char lowLevelMessage[] PROGMEM = "low level";
const char batteryMessage[] PROGMEM = "battery: ";
#endif
char sendingMessageBuf[64];

void afterSIM900SuccessInit();
void SIM900Error();

SoftwareSerial SIM900(10,11); // RX | TX
SMSProcessor sms(&SIM900, NETSTATUS_PIN, RESTART_PIN, afterSIM900SuccessInit, SIM900Error);
byte flags;
byte index;
enum Request {UNRECOGNIZED, STOP_MONITORING, GET_STATUS, CHECK_BALANCE, RESUME_MONITORING};
unsigned long timer;

#ifdef Voltage_Monitoring
int batteryLevel;                           //уровень заряда аккумулятора 0 - 499
#endif


SensorMeta sensors[] = {
  {"bathroom", 1, HUMIDITY_BATHROOM_PIN},
  {"kitchen", 1, HUMIDITY_KITCHEN_PIN},
  {"room", 1, HUMIDITY_ROOM_PIN}
};


void setup();
void softSetup();
void hardSetup();
void loop();
void stopMonitoring();
Request parseMessage(char *message);
boolean checkForAlarm(SensorMeta *sensor);
void sendStatus();
int getSensorValuesSum(SensorMeta *sensor);
void resumeMonitoring();
void checkBalance();
boolean checkBatteryLevel();
void batteryLowLevelAlarm();
void readSensorsAction();
void checkAction();
void newMessageAction();
void errorAction();
void afterRestartAction();
#endif
