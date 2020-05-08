#include <Time.h>
#include <TimeLib.h>


#include <SPI.h>
#include "SdFat.h"
#include <virtuabotixRTC.h>
SdFat SD;

#define SD_CS_PIN 10
File readFile;
File writeFile;

#define MAX_BITS 100                 // max number of bits 
#define WEIGAND_WAIT_TIME  3000      // time to wait for another weigand pulse.  

unsigned char databits[MAX_BITS];    // stores all of the data bits
unsigned char bitCount;              // number of bits currently captured
unsigned char flagDone;              // goes low when data is currently being captured
unsigned int weigand_counter;        // countdown until we assume there are no more bits

unsigned long facilityCode = 0;      // decoded facility code
unsigned long cardCode = 0;          // decoded card code

const int SIZE = 6;

int LED_GREEN = 6;
int LED_RED = 5;
int BEEP_BEEP = 4;

bool tempAdjusted = false;

bool setTheTime = false;

int relayPin = 9;

int buttonPin = 7;

bool isSuccess;

int buttonVal;

int mode = 0;

// 0 - NORMAL
// 1 - ADD
// 2 - DEL

char fileName[] = "cards.txt";

char timeFileName[] = "time.txt";

char logName[] = "log.txt";

char dstFileName[] = "dst.txt";

bool SDsuccess = false;

virtuabotixRTC myRTC(17, 18, 19);

void ISR_INT0() {
  //Serial.print("0");   // uncomment this line to display raw binary
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;

}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1() {
  //Serial.print("1");   // uncomment this line to display raw binary
  databits[bitCount] = 1;
  bitCount++;
  flagDone = 0;
  weigand_counter = WEIGAND_WAIT_TIME;
}

bool connectSD() {

  isSuccess = false;

  if (SD.begin(SD_CS_PIN)) {

    beep();

    // binds the ISR functions to the falling edge of INTO and INT1
    attachInterrupt(0, ISR_INT0, FALLING);
    attachInterrupt(1, ISR_INT1, FALLING);

    isSuccess = true;

  }

  return isSuccess;
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BEEP_BEEP, OUTPUT);
  digitalWrite(LED_RED, LOW);
  digitalWrite(BEEP_BEEP, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  pinMode(buttonPin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(2, INPUT);     // DATA0 (INT0)
  pinMode(3, INPUT);     // DATA1 (INT1)


  SDsuccess = connectSD();


  if(SDsuccess) checkTimeUpdated();

  weigand_counter = WEIGAND_WAIT_TIME;
}

void beep() {

  digitalWrite(BEEP_BEEP, LOW);
  delay(150);
  digitalWrite(BEEP_BEEP, HIGH);
  delay(150);

}

void loop()
{
  if(setTheTime){

    green();
    delay(500);
    
  }
  else if (!SDsuccess) {

    SDsuccess = connectSD();

  }
  else {

    buttonVal = digitalRead(buttonPin);

    if (buttonVal == 1) {

      if (++mode > 2)
        mode = 0;

      switch (mode) {

        case 0:
          beep();
          break;
        case 1:
          beep();
          beep();
          break;
        case 2:
          beep();
          beep();
          beep();
          break;
      }
      delay(500);

    }

    // This waits to make sure that there have been no more data pulses before processing data
    if (!flagDone) {
      if (--weigand_counter == 0)
        flagDone = 1;
    }

    // if we have bits and we the weigand counter went out
    if (bitCount > 0 && flagDone) {
      unsigned char i;

      if (bitCount == 37) {

        // standard 37 bit format
        // facility code = bits 2 to 17 (1-16)
        for (i = 1; i < 17; i++) {
          facilityCode <<= 1;
          facilityCode |= databits[i];
        }

        // card code = bits 18 to 36 (17-35)
        for (i = 17; i < 36; i++) {
          cardCode <<= 1;
          cardCode |= databits[i];
        }

        printBits();
      }

      // cleanup and get ready for the next card
      bitCount = 0;
      facilityCode = 0;
      cardCode = 0;
      for (i = 0; i < MAX_BITS; i++)
      {
        databits[i] = 0;
      }
    }
  }
}

void printBits() {

  if (mode == 1) {

    saveCard();

  }
  else if (mode == 2) {

    // del card

    deleteCard();

  }
  else {

    int resAddr = searchCard();

    if (resAddr != 0) {

      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN, LOW);

      openGarage();

      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GREEN, HIGH);



    }
    else {

      logCard(false);


      digitalWrite(LED_RED, LOW);  // Red ON
      digitalWrite(LED_GREEN, HIGH);  // Green  OFF

    }

  }
}

void red() {

  digitalWrite(LED_RED, LOW);  // Red ON
  digitalWrite(LED_GREEN, HIGH);  // Green  OFF

}

void green() {

  digitalWrite(LED_RED, HIGH);  // Red ON
  digitalWrite(LED_GREEN, LOW);  // Green  OFF
  delay(500);
  red();

}

unsigned long searchCard() {

  String currCard;

  String currStr;

  unsigned long currInt;

  char currChar;
  char lastChar;

  int index = 0;

  unsigned long foundIndex = 0;
  readFile = SD.open(fileName);
  if (readFile) {

    // read from the file until there's nothing else in it:
    while (readFile.available()) {

      currChar = readFile.read();

      if (currChar != '\n') {

        if (currChar >= '0' && currChar <= '9') {

          currCard += currChar;

          index++;

        }

      }
      else {

        char *currStr = const_cast<char*>(currCard.c_str());

        currInt = strtoul(currStr, NULL, 10);

        if (currInt == cardCode) {

          foundIndex = readFile.position() - SIZE - 1;


        }

        currCard = "";

      }
    }
  } 
  // close the file:
  readFile.flush();
  readFile.sync();
  readFile.close();

  return foundIndex;

}

void saveCard() {

  if (searchCard() == 0) {

    writeFile = SD.open(fileName, FILE_WRITE);

    char buf[16];

    ltoa(cardCode, buf, 10);

    if (writeFile) {

      writeFile.println(buf);

      writeFile.flush();
      writeFile.sync();
      writeFile.close();

      beep();
      green();

    } else {
      writeFile.close();
    }



  }
}

void deleteCard() {

  unsigned long foundIndex = searchCard();

  if (foundIndex != 0) {
    writeFile = SD.open(fileName, FILE_WRITE);

    char buf[16];

    ltoa(cardCode, buf, 10);

    if (writeFile) {

      writeFile.seek(foundIndex - 1);


      for (int i = 0 ; i < SIZE ; i++) {

        writeFile.print('-');

      }


      writeFile.flush();
      writeFile.sync();
      writeFile.close();

      beep();
      green();

    } else {
      writeFile.close();
    }

  }



}

void openGarage() {

  digitalWrite(relayPin, HIGH);

  // log date/time and cardCode in log.txt
  logCard(true);

  delay(1000); // this is in milliseconds so change this as you want
  digitalWrite(relayPin, LOW);

}

void adjustDST(int change){

  myRTC.updateTime();


  // check if doesn't go to next day
  if(((myRTC.hours + change) <= 23) && ((myRTC.hours + change) >= 0)){
    
    // seconds, minutes, hours, day of the week, day of the month, month, year
    myRTC.setDS1302Time(myRTC.seconds, myRTC.minutes, myRTC.hours + change, 
                        myRTC.dayofweek, myRTC.dayofmonth, myRTC.month, 
                        myRTC.year);  
    
  }
  
}

bool checkUpdatedDST(bool isDST){

  bool isUpdated = false;

  bool foundMonth = false;

  char currChar;

  int arrayPos = 0;

  char currYear[5];

  readFile = SD.open(dstFileName);
  if (readFile) {
    while (readFile.available()) {

      currChar = readFile.read();

      if(!isDST){
        
        if(foundMonth && arrayPos < 4){

          currYear[arrayPos] = currChar;
          
          arrayPos++;
        }

        if(foundMonth && arrayPos >=4){

          currYear[4] = '\0';

          arrayPos = 0;
          foundMonth = false;

          if(myRTC.month >= 1 && myRTC.month <= 3){
            
            if(atoi(currYear) == (myRTC.year - 1)) isUpdated = true;
          
          }
          else if(myRTC.month >= 11){

            if(atoi(currYear) == myRTC.year) isUpdated = true;
            
          }

          
        }
        
        if(currChar == 'N') foundMonth = true;
        
      }
      else {
        

        if(foundMonth && arrayPos < 4){

          currYear[arrayPos] = currChar;
          
          arrayPos++;
        }

        if(foundMonth && arrayPos >=4){

          currYear[4] = '\0';

          arrayPos = 0;
          foundMonth = false;
          if(atoi(currYear) == myRTC.year) isUpdated = true;

          
        }
        
        if(currChar == 'M') foundMonth = true;
        
      }

      
    }

    
  }

  readFile.flush();

  readFile.sync();

  readFile.close();

  return isUpdated;
  
}

void logCard(bool granted) {

  // check if in DST

  myRTC.updateTime();

  bool inDST = checkDST();

  // check if the DST has already been updated

  if(!checkUpdatedDST(inDST)){

    if(inDST){

      adjustDST(1); // add 1 hour

      storeDST(inDST); // store that we have already updated DST
      
    }
    else {

      adjustDST(-1); // subtract 1 hour

      storeDST(inDST); // store that we have already updated DST
      
    }
    
  }

    writeFile = SD.open(logName, FILE_WRITE);
  
    char buf[16];
  
    ltoa(cardCode, buf, 10);
  
    if (writeFile) {

      myRTC.updateTime();
  
      writeFile.print(myRTC.month); 
      writeFile.print("/");
      writeFile.print(myRTC.dayofmonth); //You can switch between day and month if you're using American system
      writeFile.print("/");
      writeFile.print(myRTC.year);
      writeFile.print(" ");
      writeFile.print(myRTC.hours);
      writeFile.print(":");
      if(myRTC.minutes < 10){
        writeFile.print('0');
      }
      writeFile.print(myRTC.minutes);
      writeFile.print(":");
      if(myRTC.seconds < 10){
        writeFile.print('0');
      }
      writeFile.print(myRTC.seconds);
      
      writeFile.print('\t');
      writeFile.print(buf);
      writeFile.print('\t');

      if(granted) writeFile.println("GRANTED ACCESS");
      else writeFile.println("DECLINED ACCESS");
  
    } 

    writeFile.flush();
    writeFile.sync();
    writeFile.close();

}

bool checkDST(){

    bool dst = false;
  
    int thisYear = myRTC.year;
    int thisMonth = myRTC.month;
    int thisDay = myRTC.dayofmonth;
    int thisWeekday = myRTC.dayofweek;
    int thisHour = myRTC.hours;
    int thisMinute = myRTC.minutes;
   
    if(thisMonth == 11 && thisDay < 8 && thisDay < thisWeekday)
    {
      dst=true;
    }
   
    if(thisMonth == 11 && thisDay < 8 && thisWeekday == 1 && thisHour < 2)
    {
      dst=true;
    }
  
    if(thisMonth < 11 && thisMonth > 3) dst = true;
   
    if(thisMonth == 3 && thisDay > 7 && thisDay >= (thisWeekday + 7))
    {
      if(!(thisWeekday == 1 && thisHour < 2)) dst = true;
    }

    return dst;
}

void storeDST(bool inDST){

  myRTC.updateTime();
  
  writeFile = SD.open(dstFileName, FILE_WRITE);

  if (writeFile) {

    if(inDST) {
      
      writeFile.print("M");
    }
    else {
      
      writeFile.print("N");
    }

    if(!inDST){

      if(myRTC.month >= 1 && myRTC.month <= 3){
            
        writeFile.println(myRTC.year - 1);
    
      }
      else if(myRTC.month >= 11){
    
        writeFile.println(myRTC.year);
        
      }
      
    }
    else {
      
      writeFile.println(myRTC.year);
      
    }

    

    
    
  }

  writeFile.flush();
  writeFile.sync();
  writeFile.close();
  
}

void checkTimeUpdated(){

  char currChar;

  bool startComment = false;
  bool endComment = false;
  bool startDate = false;
  bool endDate = false;

  char dayofmonth[3];
  char currmonth[3];
  char curryear[5];
  char dayofweek[3];
  char currhr[3];
  char currmin[3];
  char currsec[3];

  int arrayPos = 0;

  int currSection = 0; // 0 = days, etc.
  
  readFile = SD.open(timeFileName);
  if (readFile) {

    // Formatted as:
    // [DD, MM, YYYY, Day of week(1 is Sunday), HR, MIN, SEC]

    // read from the file until there's nothing else in it:
    while (readFile.available()) {

      currChar = readFile.read();

      if(currChar == '*'){

        if(startComment) endComment = true;
        else startComment = true;
        
      }

      if (currChar != '\n' && endComment) {

        if(currChar == '['){
  
          startDate = true;
          
        }
        else if(currChar == ']'){
  
          endDate = true;
          
        }
        else if (currChar >= '0' && currChar <= '9' && startDate && !endDate) {

          if(currSection == 2 && arrayPos > 3){ // years have 4 digits

            currSection++;
            arrayPos = 0;
            
          }
          else if(currSection != 2 && arrayPos > 1) {

            currSection++;
            arrayPos = 0;
            
          }

          if(arrayPos > 1){

            if(currSection == 2){

              if(arrayPos > 3){
                
                  currSection++;
                  arrayPos = 0;
                
              }
              
            } 
            else {

              currSection++;
              arrayPos = 0;
              
            }
            
          }

          switch(currSection){

            case 0:

              dayofmonth[arrayPos] = currChar;
              
              arrayPos++;
           
              break;
            case 1:

              currmonth[arrayPos] = currChar;
              
              arrayPos++;
           
              break;
            case 2:

              curryear[arrayPos] = currChar;
              
              arrayPos++;
           
              break;
            case 3:

              dayofweek[arrayPos] = currChar;
              arrayPos++;
           
              break;
            case 4:

              currhr[arrayPos] = currChar;
              
              arrayPos++;
           
              break;
            case 5:

              currmin[arrayPos] = currChar;
              
              arrayPos++;
           
              break;
            case 6:

              currsec[arrayPos] = currChar;
              
              arrayPos++;
           
              break;
            
          }

        }

      }

    }

    dayofmonth[2] = currmonth[2] = curryear[4] = dayofweek[2] = currhr[2]= currmin[2] = currsec[2]= '\0';

    // close the file:
      readFile.flush();
      readFile.sync();
      readFile.close();

    if(startDate && endDate){
      
      myRTC.setDS1302Time(atoi(currsec), atoi(currmin), atoi(currhr), 
                          atoi(dayofweek), atoi(dayofmonth), atoi(currmonth), atoi(curryear));

      setTheTime = true;
      
    }
    
    
  } else {

    // close the file:
      readFile.flush();
      readFile.sync();
      readFile.close();
  }
  
  
}
