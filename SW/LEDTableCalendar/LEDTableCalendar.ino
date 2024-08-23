#include <MD_MAX72xx.h>
#include <SPI.h>

#include <RTClib.h>

//#include <math.h>

using namespace std;

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define CLK_PIN   14  // or SCK
#define DATA_PIN  13  // or MOSI
#define CS_PIN    15  // or SS
#define CS_PIN2   0  // or SS
#define PIR_PIN  16   // PIR signal PIN
#define TIMESET_PIN 12

#define DEBUG_ON 1

// Parameter
#define D_COUNTER_IN_SEC 4
#define D_COUNTER_DEBOUNCE_IN_SEC 12
#define UPDATE_FROM_RTC 1000

MD_MAX72XX mx[2] = {MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES),
                    MD_MAX72XX(HARDWARE_TYPE, CS_PIN2, MAX_DEVICES)};

RTC_DS3231 rtc;
DateTime currentTime;
DateTime referenceDay(2024, 8, 24, 00, 00, 00);

// Global message buffers shared by Serial and Scrolling functions
#define BUF_SIZE  75
char message[BUF_SIZE] = "Hello!";
bool newMessageAvailable = true;
bool secondTick = true;
int statusCounter[3]{};

#define CHAR_SPACING  1 // pixels between characters

static unsigned long pir_currentTime = 0;
static unsigned long pir_triggerTime = 0;
static int state = 0;
static int preState = 0;
static bool firstCycleFlag = false;
static bool timeSet = false;
static int pirCounter = 0;
static int updateCounter = 0;

void setup() 
{
  Serial.begin(115200);

  mx[0].begin();
  mx[1].begin();
  pinMode(PIR_PIN, INPUT_PULLDOWN_16);
  pinMode(TIMESET_PIN, INPUT_PULLUP);
  timeSet = digitalRead(TIMESET_PIN);
  Serial.print("Timeset : ");
  Serial.println(uint32_t(timeSet));
  Serial.println("Begin");
  rtc.begin();
  currentTime = rtc.now();
}

void loop() 
{
  bool pirOn = digitalRead(PIR_PIN);
  pirCounter = static_cast<int>(pirOn) * ( pirCounter + 1 );
  bool dPlusActive = true;
  // Just for first activation. If you want to see longer, then detected by PIR then there is no debouncing timer
  if ( ( ( pirCounter > D_COUNTER_IN_SEC ) && ( pirCounter < D_COUNTER_DEBOUNCE_IN_SEC ) ) || ( pirCounter == 0 ) )
  {
    dPlusActive = false;
  }

  // Synchronize time to time just in case
  if (updateCounter > UPDATE_FROM_RTC)
  {
    updateCounter = 0;
    currentTime = rtc.now();
  }
  
  state = static_cast<int>(firstCycleFlag) + static_cast<int>(dPlusActive && firstCycleFlag);
  int curState = state;

  switch(curState)
  {
    // initialization
    case 0 :
      firstCycleFlag = firstCycle();
      currentTime = rtc.now();
#if defined(DEBUG_ON)
      Serial.println("case0");
#endif
      curState++;
      break;
    
    // Date calendar
    case 1 : 
      dateCalendar();
#if defined(DEBUG_ON)
      Serial.println("case1");
#endif
      break;

    // D++ counter init
    case 2 : 
      dayPlusCounter();
#if defined(DEBUG_ON)
      Serial.println("case2");
#endif
      break;
    
    // Do nothing
    case 3 : 
      // Do nothing
#if defined(DEBUG_ON)
      Serial.println("case3");
#endif
      break;

    // Other case, go to default mode. Date calendar
    default : 
      curState = 1;
      break;
  }
  preState = state;
  delay(1000);
  TimeSpan adding1s = 1;
  currentTime = currentTime + adding1s;
  updateCounter++;
#if defined(DEBUG_ON)
  Serial.print("TimeSet Pin : ");
  Serial.println(digitalRead(TIMESET_PIN));
  Serial.print("PIR Counter : ");
  Serial.println(pirCounter);
  Serial.print("cyc : ");
  Serial.println(state);
#endif
}

bool firstCycle()
{
  // Time set mode
  if(timeSet)
  {  
    // Clear the LED
    clearDisplay();

    // LED print page1
    ledPrintPageInit();

    Serial.println("First Cycle, timeset_pin");
    DateTime curDateTime = rtc.now();
    
    Serial.println("Current RTC Time : ");

    Serial.print(curDateTime.year());
    Serial.print(curDateTime.month());
    Serial.print(curDateTime.day());
    Serial.print(curDateTime.hour());
    Serial.print(curDateTime.minute());
    Serial.println(curDateTime.second());
    String inputString;
    char newData[14]{};
    bool notCompleted = true;

    Serial.println("Please write current time as YYYYMMDDHHmmss 2024.08.24 14:20:00, then please type 20240824142000 : ");
    // Receive the string
    do 
    {
      while(!Serial.available()) {};
      inputString = Serial.readString();
      Serial.println(inputString);
      Serial.println(inputString.length());
      if(inputString == "Y" || inputString == "y") 
      {
        return true;
      }
      // Bound check
      if(inputString.length() == 16) 
      {
        Serial.println("Number of input is good");
        for(int idx = 0; idx < 14; idx++)
        {
          newData[idx] = inputString.charAt(idx)-'0';
          if(newData[idx] < 0 && newData[idx] > 9) 
          {
            break;
          }
          if(idx == 13) 
          {
            Serial.println("Good input");
            notCompleted = false;
          }
        }
      }
    } while(notCompleted);

    Serial.println(newData);
    
    uint16_t YYYY = newData[0]*1000 + newData[1]*100 + newData[2]*10 + newData[3];
    uint8_t MM = newData[4]*10 + newData[5];
    uint8_t DD = newData[6]*10 + newData[7];
    uint8_t HH = newData[8]*10 + newData[9];
    uint8_t mm = newData[10]*10 + newData[11];
    uint8_t ss = newData[12]*10 + newData[13];
    DateTime timeSet(YYYY, MM, DD, HH, mm, ss);
    
    if(timeSet.isValid()) 
    {
      Serial.println("RTC Time set");
      rtc.adjust(timeSet);
      return true;
    }
    else 
    {
      Serial.println("Invalid data time");
      return false;
    }
  }
  else
  {
    return true;
  }
  return false;
}

// | MM.DD |
// | HH:MM |
void dateCalendar()
{
  // Clear the LED
  clearDisplay();

  // LED print page1
  ledPrintPage1();
}

// |  R&O  |
// | 00000 |
void dayPlusCounter()
{
  // Clear the LED
  clearDisplay();

  // LED print page2
  ledPrintPage2();
}

void clearDisplay()
{
  for(int idx = 0; idx < 2; idx++)
  {
    mx[idx].clear();
  }
}

// YEAR/TIME
void ledPrintPageInit() 
{
  
  uint32_t month = static_cast<uint32_t>(currentTime.month());
  uint32_t day = static_cast<uint32_t>(currentTime.day());
  uint32_t hour = static_cast<uint32_t>(currentTime.hour());
  uint32_t minute = static_cast<uint32_t>(currentTime.minute());

  char offset = '0';
  char printL1[7] = { 'I', 'N', 'I', 'T', ':', ')', '\0'};
  // MONTH/DAY
  printText(mx[0], 0, MAX_DEVICES-1, &printL1[0],2);
#if defined(DEBUG_ON)
  Serial.println("ledprint init");
#endif
}

// YEAR/TIME
void ledPrintPage1() 
{
  
  uint32_t month = static_cast<uint32_t>(currentTime.month());
  uint32_t day = static_cast<uint32_t>(currentTime.day());
  uint32_t hour = static_cast<uint32_t>(currentTime.hour());
  uint32_t minute = static_cast<uint32_t>(currentTime.minute());

  char offset = '0';
  char printL1[6] = { char((month/10) + offset), char((month - (month/10) * 10) + offset), '.', char((day/10) + offset), char((day - (day/10) * 10) + offset), '\0'};
  char printL2[6] = { char((hour/10) + offset), char((hour - (hour/10) * 10) + offset), ':', char((minute/10) + offset), char((minute - (minute/10) * 10) + offset), '\0'}; 
  if(secondTick)
  {
    printL2[2] = ' '; 
  }
  secondTick = !secondTick;

  // MONTH/DAY
  printText(mx[0], 0, MAX_DEVICES-1, &printL1[0],1);
  // HOUR/MINUTE
  printText(mx[1], 0, MAX_DEVICES-1, &printL2[0],1);
#if defined(DEBUG_ON)
  Serial.println(printL1);
  Serial.println(printL2);

  Serial.println("ledprint1 ran");
#endif
}

// D+ Counter
void ledPrintPage2() 
{
  DateTime curDateTime = rtc.now();
  int deltaSec = curDateTime.unixtime() - referenceDay.unixtime();
  int deltaDay = int(deltaSec / (3600*24)) + 1;
  char offset = '0';
  // Digit calculation
  uint32_t digit = 0;
  uint32_t currentDigit = 5;
  uint32_t curNumDigit = 0;
  uint32_t power = 100000;
  do
  {
    currentDigit--;
    power /= 10;
    curNumDigit = uint32_t(deltaDay / power);
  }
  while(curNumDigit==0);
  currentDigit++;

  char printL1[6] = { ' ','R', '&', 'O', ' ', '\0' };
  char printL2[6]{};
  char input[6] = { char(deltaDay/10000) + offset, char((deltaDay/1000) - (deltaDay/10000) * 10 ) + offset, char((deltaDay/100) - (deltaDay/1000) * 10) + offset, char((deltaDay/10) - (deltaDay/100) * 10) + offset, char(deltaDay - (deltaDay/10) * 10) + offset };
  removeZero(&input[0], &printL2[0]);
  if(deltaDay < 10000 && deltaDay > 0)
  {
    printL2[0] = 'D';
  }
  else if(deltaDay >= 10000)
  {
    // Not using 'D' in the beginning for the 5 digit display
  }
  else 
  {
    // set '0' if minus or out of boundary
    printL2[0] = 'D';
    printL2[1] = ' ';
    printL2[2] = ' ';
    printL2[3] = ' ';
    printL2[4] = '0';
    printL2[5] = '\0';
  }

  printText(mx[0], 0, MAX_DEVICES-1, &printL1[0],2);
  printText(mx[1], 0, MAX_DEVICES-1, &printL2[0],2);

#if defined(DEBUG_ON)
  Serial.print("Delta time in days : ");
  Serial.println(deltaDay);
  Serial.print("input : ");
  Serial.println(input);
  Serial.print("printL2 : ");
  Serial.println(printL2);
  Serial.print("currentDigi : ");
  Serial.println(currentDigit);
#endif
}

void removeZero(char* input, char* output)
{
  bool firstDigit = false;
  for (int idx = 0; idx < 6; idx++)
  {
    if(input[idx] != '0' || firstDigit)
    {
      firstDigit = true;
      output[idx] = input[idx];
    }
    else
    {
      output[idx] = ' ';
    }
  }
}

void rightAlign(char* input, char* output, uint32_t numValidInput, uint32_t numOutput)
{
  uint32_t arrIdx = numOutput - numValidInput - 1;
  uint32_t currentInputIdx = numValidInput - 1;
  for (int idx = arrIdx; idx < numOutput; idx++)
  {
    output[idx] = input[currentInputIdx];
    currentInputIdx++;
  }
}


void printText(MD_MAX72XX& deviceId, uint8_t modStart, uint8_t modEnd, char *pMsg, uint8_t frontSpacing)
// Print the text string to the LED matrix modules specified.
// Message area is padded with blank columns after printing.
{
  uint8_t   state = 0;
  uint8_t   curLen;
  uint16_t  showLen;
  uint8_t   cBuf[8];
  int16_t   col = ((modEnd + 1) * COL_SIZE) - 1;

  deviceId.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  for(uint8_t idx = 0; idx < frontSpacing; idx++)
  {
    deviceId.setColumn(col--, 0);
  }
  
  do     // finite state machine to print the characters in the space available
  {
    switch(state)
    {
      case 0: // Load the next character from the font table
        // if we reached end of message, reset the message pointer
        if (*pMsg == '\0')
        {
          showLen = col - (modEnd * COL_SIZE);  // padding characters
          state = 2;
          break;
        }

        // retrieve the next character form the font file
        showLen = deviceId.getChar(*pMsg++, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);

        curLen = 0;
        state++;
        // !! deliberately fall through to next state to start displaying

      case 1: // display the next part of the character
        deviceId.setColumn(col--, cBuf[curLen++]);

        // done with font character, now display the space between chars
        if (curLen == showLen)
        {
          showLen = CHAR_SPACING;
          state = 2;
        }
        break;

      case 2: // initialize state for displaying empty columns
        curLen = 0;
        state++;
        // fall through

      case 3:	// display inter-character spacing or end of message padding (blank columns)
        deviceId.setColumn(col--, 0);
        curLen++;
        if (curLen == showLen)
          state = 0;
        break;

      default:
        col = -1;   // this definitely ends the do loop
    }
  } while (col >= (modStart * COL_SIZE));

  deviceId.control(modStart, modEnd, MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}