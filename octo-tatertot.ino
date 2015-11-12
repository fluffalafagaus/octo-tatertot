#include <Wire.h>
#include <Time.h>
#include <DS1307RTC.h>
#include <string.h>
#include <Thread.h>
#include <ThreadController.h>

#define alarm1 1
#define alarm2 20
#define alarm2end 40

//ThreadController threadManager = ThreadController();
//Thread alarmCheck = Thread();

void setup(){
    Serial.begin(9600);
    delay(1000);
}

void loop(){
    tmElements_t tm;    
    tmElements_t alarm;

    alarm.Day = 12; alarm.Month = 11; alarm.Year = CalendarYrToTm(2015); alarm.Hour = 14; alarm.Minute = 37;

    RTC.read(tm);
    Serial.print("Ok, Time = ");
    print2digits(tm.Hour);
    Serial.write(':');
    print2digits(tm.Minute);
    Serial.write(':');
    print2digits(tm.Second);
    Serial.print(", Date (D/M/Y) = ");
    Serial.print(tm.Day);
    Serial.write('/');
    Serial.print(tm.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(tm.Year));
    Serial.println();
    Serial.print("Reading:");
    for(int i = 0; i <= 48; i++){
        Serial.print(char(readRam(i)));
    }
    Serial.print("\r\nAlarmCheck(0-3): ");
    Serial.println(int(checkAlarms()));

    delay(1000);
}

void print2digits(int number) {
    if (number >= 0 && number < 10) {
        Serial.write('0');
    }
    Serial.print(number);
}
void writeRam(int address,byte data){
  Wire.beginTransmission(0x68);              // Select DS1307
  Wire.write(address+8);                       // address location starts at 8, 0-6 are date, 7 is control
  Wire.write(data);                            // send data
  Wire.endTransmission();
}
byte readRam(int address){  
  Wire.beginTransmission(0x68);
  Wire.write(address+8);
  Wire.endTransmission();
  Wire.requestFrom(0x68,1);
  uint8_t data = Wire.read();
  Wire.endTransmission();
  return data;
}
uint8_t setAlarm(tmElements_t time_){
  char alarmFormat[19];
  uint8_t active = readRam(0);
  snprintf(alarmFormat, sizeof(alarmFormat), "#*%u/%u/%u@%u:%u;",time_.Day, time_.Month, time_.Year, time_.Hour, time_.Minute);//format string into correct format for ram
  switch(active){
    case 0://no alarms
      writeRam(0,0b00000001);
      for(int i = 0; i <= sizeof(alarmFormat); i++){
        writeRam(i+alarm1, alarmFormat[i]);
      }
      break;
    case 1://1st alarm set and active; write into slot 2
      writeRam(0,0b00000011);
      for(int i = 0; i <= sizeof(alarmFormat); i++){
        writeRam(i+alarm2, alarmFormat[i]);
      }
      break;
    case 2://2nd alarm set and active; write into slot 1
      writeRam(0,0b00000011);
      for(int i = 0; i <= sizeof(alarmFormat); i++){
        writeRam(i+alarm2, alarmFormat[i]);
      }
      break;
    case 3://both alarms set and active; exit
      return -1;
      break;
    /*default://alarm byte not set; write into slot 1
      writeRam(0,0b00000001);
      for(int i = 0; i <= sizeof(alarmFormat); i++){
        writeRam(i+alarm1, alarmFormat[i]);
      }
      break;*/
  }
}
uint8_t checkAlarms(){
    char  alarmBuff[25];
    uint8_t alarmRing[2]={0,0};
    uint8_t alarmByteOld=readRam(0);
    int index = 0;
    tmElements_t current_time;
    tmElements_t alarm_time;
    if(readRam(0)==0b00000001||readRam(0)==0b00000011){
        for(int i = 0;i <= 25;i++){
            alarmBuff[i] = readRam(i);      
        }
        alarm_time = alarmStringtoTm(alarmBuff);
        RTC.read(current_time);
        if(hasTimeOccurred(current_time, alarm_time)){
            alarmRing[0]=1;
        }
    }
    if(readRam(0)==0b00000010||readRam(0)==0b00000011){
        for(int i = 0;i <= 25;i++){
            alarmBuff[i] = readRam(i+19);      
        }
        alarm_time = alarmStringtoTm(alarmBuff);
        if(hasTimeOccurred(current_time, alarm_time)){
            alarmRing[1]=1;
        }
    }
    if(alarmRing[0]==1&&alarmRing[1]==1){   
        for(int i=1;i<=50;i++){
            writeRam(i,0);
        }
        writeRam(0,0b00000000);
        return 3;
    }else if(alarmRing[0]==1&&alarmRing[1]==0){
        for(int i=1;i<=18;i++){
            writeRam(i,0);
        }
        writeRam(0,(alarmByteOld & 0b00000010));
        return 1;
    }else if(alarmRing[0]==0&&alarmRing[1]==1){
        for(int i=19;i<=50;i++){
            writeRam(i,0);
        }
        writeRam(0,(alarmByteOld & 0b00000001));
        return 2;
    }else if(alarmRing[0]==0&&alarmRing[1]==0){
        return 0;
    }

}   
uint8_t hasTimeOccurred(tmElements_t curtime, tmElements_t chktime){
    if(curtime.Year>chktime.Year){//<can be true^
        return 1;
    }else if(curtime.Year==chktime.Year){
        if(curtime.Month>chktime.Month){
            return 1;
        }else if(curtime.Month==chktime.Month){
            if(curtime.Day>chktime.Day){
                return 1;
            }else if(curtime.Day==chktime.Day){
                if(curtime.Hour>chktime.Hour){
                    return 1;
                }else if(curtime.Hour==chktime.Hour){
                    if(curtime.Minute>chktime.Minute){
                        return 1;
                    }else if(curtime.Minute==chktime.Minute){
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
tmElements_t alarmStringtoTm(char astr[25]){
    char day[3]={'0','0','\0'};
    char month[3]={'0','0','\0'};
    char year[3]={'0','0','\0'};
    char hour[3]={'0','0','\0'};
    char minute[3]={'0','0','\0'};
    int index = 0;
    tmElements_t alarm_time;

    while(!(astr[index] =='*')){
        index++;
    }
    index++;
    for(int i=0;i<=1;i++){//Day
        day[i]=astr[index];
        index++;
    }index++;
    for(int i=0;i<=1;i++){//Month
        month[i]=astr[index];
        index++;
    }index++;
    for(int i=0;i<=1;i++){//Year
        year[i]=astr[index];
        index++;
    }index++;
    for(int i=0;i<=1;i++){//Hour
        hour[i]=astr[index];
        index++;
    }index++;
    for(int i=0;i<=1;i++){//Minute
        minute[i]=astr[index];
        index++;
    }

    alarm_time.Day=atoi(day);
    alarm_time.Month=atoi(month);
    alarm_time.Year=atoi(year);
    alarm_time.Hour=atoi(hour);
    alarm_time.Minute=atoi(minute);

    return alarm_time;
}
