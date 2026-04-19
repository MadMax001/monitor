#ifndef GSMCommandProcessor_H
#define GSMCommandProcessor_H

//#define DEBUG_GSMCommandProcessor

#include <SoftwareSerial.h>

#define ANSWER_TIME_MILLIS 1000                       //полное время ожидания ответа
#define ANSWER_BUFF_SIZE 128                           //размер буфера ответа

#define WAITING_ANSWER_FLAG 0                        //побитовый флаг, показывающий, что в данный момент ожидается ответ
#define SENDING_COMMAND_FLAG 1                       //побитовый флаг, показывающий, что в данный момент идет отсыл команды, ответ не ожидаем

class GSMCommandProcessor {
  private:
    SoftwareSerial *module;
    byte flags;
    char fromModuleData[ANSWER_BUFF_SIZE];            //буфер, куда "складывается" ответ
    int dataIndex;                                    //позиция следующего символа ответа для помещения в буфер
    unsigned long answerTimer;
    unsigned long timeout;
    char c = ' ';
    char *lastCommand;                                //команда
    void makeAnswer();
    
  public:
    GSMCommandProcessor(SoftwareSerial* SIM900);
    void sendATcommand(char *command, unsigned long _timeout = ANSWER_TIME_MILLIS);
    boolean canGetAnswer();
    boolean canSendCommand();
    char* getAnswer();
    void listenPort(unsigned long _timeout = ANSWER_TIME_MILLIS);
    
    void step();
};

#endif 
