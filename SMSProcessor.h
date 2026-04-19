#ifndef SMSProcessor_H
#define SMSProcessor_H

//#define DEBUG_SMSProcessor

#include <SoftwareSerial.h>
#ifdef DEBUG_SMSProcessor
#include <MemoryFree.h>
#endif

#include "GSMCommandProcessor.h"
#include "ProjectUtils.h"

#define LED_IS_ON 1
#define LED_IS_OFF 0
#define MAX_ATTEMPTS_FOR_COMMAND_REPEAT 5             //число повтора неудачной AT-команды
#define POWER_KEY_DOWN_DURATION 2500                  //время нажатии на кнопку POWER при включении модема

#define EMPTY_FLAG -1                                 //заглушка
#define RECEIVING_ANSWER_FLAG 0                       //побитовый флаг, отвечающий за то, что в данный момент идет получение ответа
#define INITIALIZATION_COMPLETE_FLAG 1                //побитовый флаг, показывающий, что процесс инициализации завершен
#define BUSY_FLAG 2                                   //побитовый флаг, показывающий, что в данный момент присходит выполнение команд
#define ERROR_FLAG 3                                  //побитовый флаг, указывающий на ошибку при выполнении команды
#define SENDING_FLAG 4                                //побитовый флаг, указывающий, что идет процесс отправки сообщений
#define READING_FLAG 5                                //побитовый флаг, указывающий, что идет процесс чтения сообщений
#define NEW_MESSAGE_FLAG 6                            //побитовый флаг, указывающий на непрочитанное сообщение, которое может быть прочитано
#define NEW_MESSAGE_EXISTS_FLAG 7                     //побитовый флаг, указывающий на обнаружение непрочитанного сообщения (пока его еще нельзя прочитать, т.к. процесс подготовки не завершен)
#define CHECK_MESSAGES_FLAG 8                         //побитовый флаг, указывающий на процесс чтения списка всех сообщений
#define POWER_OFF_COMPLETE_FLAG 9                     //побитовый флаг, указывающий на завершение процесса выключения
#define POWER_ON_COMPLETE_FLAG 10                     //побитовый флаг, указывающий на завершение процесса включения

using CallbackFunction = void (*)(void);

class SMSProcessor {
  private:
    GSMCommandProcessor *atProcessor;            //объект для отправки AT-команд и получения ответа
    enum ProcessMode {NONE, INIT, SEND, READ, CHECK, BALANCE, POWEROFF, POWERON} mode;
    enum NetStatus {UNDEFINED, NOT_REGISTERED, REGISTERED, GPRS};
    int flags;
    char commandBuf[23];                        
    char messageNum[1];
    char *messageBuf;
    char *newMessageBuf;
    byte commandIndex;                           //индекс в массиве команд
    byte attempts;                               //количество неудачных попыток (когда ответ не совпадает с ожидаемым ответом)
    byte messageIndex;                           //индекс сообщения 
    byte restartPin;                              //номер вывода для управления перезупуском модема
    static byte netStatusPin;                    //номер вывода для отслеживания сетевого статуса модема
    static byte netStatusInt;                    //номер прерывания для отслеживания сетевого статуса модема
    static volatile unsigned long watchTimer;                    
    static volatile int netStatusPahseDuration[2];               //массив для хранения длительностей горения и негорения диода сетевого статуса, 0 - не горит, 1 - горит    
    unsigned long timer;
    
    CallbackFunction callbackAfterSuccessInitialization;  //функция, которая вызывается после инициализации
    CallbackFunction callbackAfterError;                  //функция, которая вызывается после ошибки
    
    boolean isBusy();    
    void initCommandsSendingStep();
    void initCommandsAnswerReceivingStep(char *answer);
    void sendCommandsSendingStep();
    void sendCommandsAnswerReceivingStep(char *answer);
    void readCommandsSendingStep();
    void readCommandsAnswerReceivingStep(char *answer);
    void checkCommandsSendingStep();
    void balanceCommandsSendingStep();
    void balanceCommandsAnswerReceivingStep(char *answer);
    void powerOffCommandSendingStep();
    void powerOffCommandsAnswerReceivenigStep();
    void powerOnPWRKeyDown();
    void powerOnPWRKeyUp();

    void blockFinish(byte bitNumber = EMPTY_FLAG, boolean _error = false);
    void blockBegin(ProcessMode _mode, byte bitNumber = EMPTY_FLAG);
    void unsuccessfulAttempt(char *answer, byte bitNumber = EMPTY_FLAG);

    boolean checkForNetRegistration(char *answer);
    void getMessage(char *cellNumber);
    NetStatus getNetStatus();
  public:
    SMSProcessor(SoftwareSerial *SIM900, byte netStatusPin, byte restartPin, CallbackFunction callbackAfterSuccessInitialization = NULL, CallbackFunction callbackAfterError = NULL);
    void step();
    boolean isReady();
    boolean isError();
    void init();
    boolean isInitializationComplete();
    boolean isNewMessage();
    char* readMessage();
    void sendMessage(char *messsage);
    void checkBalance();
    void checkMessages();
    void startNetStatusWatching();
    void stopNetStatusWatching();
    static void changeNetStatusPhase();
    boolean isNetRegistered();
    void restart();
    boolean isRestarting();
};

    const char commandInit1[] PROGMEM = "AT+IPR=9600";
    const char commandInit2[] PROGMEM = "AT";
    const char commandInit3[] PROGMEM = "AT+CMGF=1";
    const char commandInit4[] PROGMEM = "AT+CSCS=\"GSM\"";
    const char commandInit5[] PROGMEM = "AT+CNMI=2,1,0,0,0";
    const char commandDeleteAllMessages[] PROGMEM = "AT+CMGDA=\"DEL ALL\"";
    //const char commandInit6[] PROGMEM = "AT+CPMS=\"SM\"";         //емкость по сообщениям на SIM
    const char commandInit6[] PROGMEM = "AT+CSCLK=2";
    const char commandInit7[] PROGMEM = "AT+CREG?";

    const char* const initCommands[] PROGMEM = {commandInit1, commandInit1, commandInit2, commandInit3, commandInit4, commandInit5, commandDeleteAllMessages, commandInit6, commandInit7};

    const char commandMessageRead[] PROGMEM = "AT+CMGR=";
    const char commandMessageSend[] PROGMEM = "AT+CMGS=";
    const char commandMessageDelete[] PROGMEM = "AT+CMGD=";
    const char phoneNumber[] PROGMEM = "\"+79873053808\"";
    const char commandCheckBalance[] PROGMEM = "ATD#100#";

    const char commandPowerOff[] PROGMEM = "AT+CPOWD=1";

    const char OKAnswer[] PROGMEM = "OK";
    const char ERRORAnswer[] PROGMEM = "ERROR";
    const char SMSSendAnswer[] PROGMEM = "> ";
    const char RegAnswerPrefix[] PROGMEM = "+CREG:"; 
    const char InMessageAnswerPrefix[] PROGMEM = "+CMTI:";
    const char InCallAnswerPrefix[] PROGMEM = "RING";

#endif
