/*
  KISS OSD CONFIG TOOL
  by Alexander Wolf (awolf78@gmx.de)
  for everyone who loves KISS

  Anyone is free to copy, modify, publish, use, compile, sell, or
  distribute this software, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any
  means.

  In jurisdictions that recognize copyright laws, the author or authors
  of this software dedicate any and all copyright interest in the
  software to the public domain. We make this dedication for the benefit
  of the public at large and to the detriment of our heirs and
  successors. We intend this dedication to be an overt act of
  relinquishment in perpetuity of all present and future rights to this
  software under copyright law.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  OTHER DEALINGS IN THE SOFTWARE.

  For more information, please refer to <http://unlicense.org>
*/




// CONFIGURATION

// motors magnepole count (to display the right RPMs)
//=============================
#define MAGNETPOLECOUNT 14 // 2 for ERPMs

// Filter for ESC datas (higher value makes them less erratic) 0 = no filter, 20 = very strong filter
//=============================
#define ESC_FILTER 10

/* Low battery warning config
  -----------------------------
  This feature will show a flashing "BATTERY LOW" warning just below the center of your
  display when bat_mAh_warning is exceeded - letting you know when it is time to land.
  I have found the mAh calculation of my KISS setup (KISS FC with 24RE ESCs using ESC telemetry) to
  be very accurate, so be careful when using other ESCs - mAh calculation might not be as accurate.
  While disarmed, yaw to the right. This will display the selected battery. Roll left or right to
  select a different battery. Pitch up or down to change the battery capacity. The warning will be
  displayed according to the percentage configured in the menu. Enter the menu with a yaw to the left
  for more than 3 seconds. Default is 23%, so if you have a 1300 mAh battery, it would start warning
  you at 1001 mAh used capacity.
  =============================*/
static const int16_t BAT_MAH_INCREMENT = 50;

// END OF CONFIGURATION
//=========================================================================================================================


// internals
//=============================

//#define DEBUG
//#define PROTODEBUG
#define KISS_OSD_CONFIG

const char KISS_OSD_VER[] = "kiss osd config v2.3";

#include <SPI.h>
#include "MAX7456.h"
#include "Max7456Config.h"
#include <EEPROM.h>
#include "SerialPort.h"
#include "CSettings.h"
#include "CStickInput.h"
#include "fixFont.h"
#include "Config.h"

#ifdef IMPULSERC_VTX
const uint8_t osdChipSelect          =            10;
#else
const uint8_t osdChipSelect          =            6;
#endif
const byte masterOutSlaveIn          =            MOSI;
const byte masterInSlaveOut          =            MISO;
const byte slaveClock                =            SCK;
const byte osdReset                  =            2;

CMax7456Config OSD( osdChipSelect );
SerialPort<0, 64, 0> NewSerial;
CStickInput inputChecker;
CSettings settings;
static int16_t DV_PPMs[CSettings::DISPLAY_DV_SIZE];
static bool OSD_ITEM_BLINK[CSettings::OSD_ITEMS_SIZE];
static uint8_t itemLengthOK[CSettings::OSD_ITEMS_POS_SIZE];
static bool correctItemsOnce = false;

#ifdef IMPULSERC_VTX
static volatile uint8_t vTxChannel, vTxBand, vTxPower, oldvTxChannel, oldvTxBand;
const uint8_t VTX_BAND_COUNT = 5;
const uint8_t VTX_CHANNEL_COUNT = 8;
//static unsigned long changevTxTime = 0;
static const char bandSymbols[][2] = { {'a', 0x00} , {'b', 0x00}, {'e', 0x00}, {'f', 0x00}, {'r', 0x00}};

const uint16_t vtx_frequencies[VTX_BAND_COUNT][VTX_CHANNEL_COUNT] PROGMEM = {
  { 5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725 }, //A
  { 5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866 }, //B
  { 5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945 }, //E
  { 5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880 }, //F
  { 5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917 }  //R
};

void setvTxSettings()
{
  oldvTxChannel = vTxChannel = settings.m_vTxChannel;
  oldvTxBand = vTxBand =  settings.m_vTxBand;
  vTxPower = settings.m_vTxPower;
}
#endif

void cleanScreen()
{
  OSD.clear();
  while (!OSD.clearIsBusy());
}

static uint8_t lastTempUnit;

void setupMAX7456()
{
  OSD.begin(settings.COLS,13);
  delay(100);
  settings.m_videoMode = OSD.videoSystem();
  if(settings.m_videoMode == MAX7456_PAL) settings.ROWS = 15;
  else settings.ROWS = 13;
  OSD.setTextArea(settings.COLS, settings.ROWS);  
  OSD.setDefaultSystem(settings.m_videoMode);
  OSD.setSwitchingTime( 5 );   
  OSD.setCharEncoding( MAX7456_ASCII );  
  OSD.display();
#ifdef IMPULSERC_VTX
  delay(100);
  MAX7456Setup();
#endif
  delay(100);
  OSD.setTextOffset(settings.m_xOffset, settings.m_yOffset);
}

void resetBlinking()
{
  uint8_t i = 0;
  for (i = 0; i < CSettings::OSD_ITEMS_SIZE; i++)
  {
    OSD_ITEM_BLINK[i] = false;
  }
}


void setup()
{
  SPI.begin();
  SPI.setClockDivider( SPI_CLOCK_DIV2 );  
  setupMAX7456();
  settings.cleanEEPROM();
  settings.ReadSettings();
  OSD.setTextOffset(settings.m_xOffset, settings.m_yOffset);

  //clean used area
  while (!OSD.notInVSync());
  cleanScreen();
#ifdef IMPULSERC_VTX
  //Ignore setting because this is critical to making sure we can detect the
  //VTX power jumper being installed. If we aren't using 5V ref there is
  //the chance we will power up on wrong frequency.
  analogReference(DEFAULT);
  setvTxSettings();
  vtx_init();
  vtx_set_frequency(vTxBand, vTxChannel);
  vtx_flash_led(5);
#endif
  lastTempUnit = settings.m_tempUnit;
  settings.SetupPPMs(DV_PPMs);
  resetBlinking();
  uint8_t i;
  for (i = 0; i < CSettings::OSD_ITEMS_POS_SIZE; i++) itemLengthOK[i] = 0;
  NewSerial.begin(115200);
}

static int16_t  throttle = 0;
static int16_t  roll = 0;
static int16_t  pitch = 0;
static int16_t  yaw = 0;
static uint16_t current = 0;
static int8_t armed = 0;
static int8_t idleTime = 0;
static int16_t LipoVoltage = 0;
static int16_t LipoMAH = 0;
static int16_t  previousMAH = 0;
static int16_t totalMAH = 0;
static int16_t MaxAmps = 0;
static int16_t MaxC    = 0;
static int16_t MaxRPMs = 0;
static int16_t MaxWatt = 0;
static int16_t MaxTemp = 0;
static int16_t MinBat = 0;
static int16_t motorKERPM[4] = {0, 0, 0, 0};
static int16_t maxKERPM[4] = {0, 0, 0, 0};
static int16_t motorCurrent[4] = {0, 0, 0, 0};
static int16_t maxCurrent[4] = {0, 0, 0, 0};
static int16_t ESCTemps[4] = {0, 0, 0, 0};
static int16_t maxTemps[4] = {0, 0, 0, 0};
static int16_t ESCVoltage[4] = {0, 0, 0, 0};
static int16_t minVoltage[4] = {10000, 10000, 10000, 10000};
static int16_t ESCmAh[4] = {0, 0, 0, 0};
static int16_t  AuxChanVals[4] = {0, 0, 0, 0};
static unsigned long start_time = 0;
static unsigned long time = 0;
static unsigned long old_time = 0;
static unsigned long total_time = 0;
static unsigned long DV_change_time = 0;
static int16_t last_Aux_Val = -10000;
static boolean armedOnce = false;
static boolean showBat = false;
static boolean settingChanged = false;
static uint8_t statPage = 0;


uint32_t ESC_filter(uint32_t oldVal, uint32_t newVal) {
  return (uint32_t)((uint32_t)((uint32_t)((uint32_t)oldVal * ESC_FILTER) + (uint32_t)newVal)) / (ESC_FILTER + 1);
}

int16_t findMax4(int16_t maxV, int16_t *values, int16_t length)
{
  for (uint8_t i = 0; i < length; i++)
  {
    if (values[i] > maxV)
    {
      maxV = values[i];
    }
  }
  return maxV;
}


int16_t findMax(int16_t maxV, int16_t newVal)
{
  if (newVal > maxV) {
    return newVal;
  }
  return maxV;
}

int16_t findMin(int16_t minV, int16_t newVal)
{
  if (newVal < minV) {
    return newVal;
  }
  return minV;
}

void ReviveOSD()
{
  while (OSD.resetIsBusy()) delay(100);
  OSD.reset();
  while (OSD.resetIsBusy()) delay(100);
  setupMAX7456();
}

static uint32_t LastLoopTime = 0;

typedef void* (*fptr)();
static char tempSymbol[] = {0xB0, 0x00};
static char ESCSymbol[] = {0x7E, 0x00};
static uint8_t code = 0;
static boolean menuActive = true;
static boolean menuWasActive = false;
extern void* MainMenu();
static void* activePage = (void*)MainMenu;
static boolean batterySelect = false;
static boolean shiftOSDactive = false;
static boolean triggerCleanScreen = true;
static uint8_t activeMenuItem = 0;
static uint8_t stopWatch = 0x94;
static char batterySymbol[] = { 0xE7, 0xEC, 0xEC, 0xED, 0x00 };
static uint8_t krSymbol[4] = { 0x9C, 0x9C, 0x9C, 0x9C };
static unsigned long krTime[4] = { 0, 0, 0, 0 };
static boolean saveSettings = false;
#ifdef UPDATE_FONT_ONLY
static boolean updateFont = true;
#else
static boolean updateFont = false;
#endif
static boolean updateFontComplete = false;
static boolean moveItems = false;
static uint8_t moveSelected = 0;
static unsigned long startMoveTime = 0;
static boolean setupNickname = false;
static const uint8_t charTable[] = { 0x20, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,
                                     0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33, 0x34,
                                     0x35, 0x36, 0x37, 0x38, 0x39
                                   };
static const uint8_t charTableSize = 37;
static uint8_t charSelected = 0;
static uint8_t charIndex = 1;
volatile bool timer1sec = false;
static unsigned long timer1secTime = 0;
static uint8_t currentDVItem = 0;
static bool symbolOnOffChanged = false;
static uint16_t fcNotConnectedCount = 0;

#ifdef DEBUG
static int16_t versionProto = 0;
#endif

static unsigned long _StartupTime = 0;
extern void* MainMenu();

uint8_t findCharPos(uint8_t charToFind)
{
  if (charToFind == 0x00) return 1;
  uint8_t i = 0;
  while (charTable[i] != charToFind && i < charTableSize) i++;
  return i;
}

void loop() {
  uint8_t i = 0;
  static uint8_t blink_i = 1;
  unsigned long currentMillis = millis();

#ifdef IMPULSERC_VTX
  /*if(oldvTxBand != vTxBand || oldvTxChannel != vTxChannel || changevTxTime > 0)
    {
    if(changevTxTime == 0)
    {
      changevTxTime = millis();
      oldvTxBand = vTxBand;
      oldvTxChannel = vTxChannel;
    }
    }
    else
    {*/
  vtx_process_state(millis(), vTxBand, vTxChannel);
  //}
#endif

  if (_StartupTime == 0)
  {
    _StartupTime = millis();
  }

  if ((millis() - timer1secTime) > 500)
  {
    timer1secTime = millis();
    timer1sec = !timer1sec;
  }

  if ((micros() - LastLoopTime) > 10000)
  {
    LastLoopTime = micros();

#ifdef IMPULSERC_VTX
    if (timer1sec)
    {
      vtx_set_power(armed ? vTxPower : 0);
    }
#endif

#ifdef DEBUG
    while (!OSD.notInVSync());
    uint8_t debug_col = 8;
    OSD.printInt16(debug_col, 8, (int16_t)analogRead(A6), 0, 0);
#endif

    NewSerial.write(0x20); // request telemetry
    if (!ReadTelemetry())
    {
      fcNotConnectedCount++;
      if (fcNotConnectedCount <= 500) return;
    }
    else fcNotConnectedCount = 0;

    while (!OSD.notInVSync());

    if (fcNotConnectedCount > 500)
    {
      static const char FC_NOT_CONNECTED_STR[] PROGMEM = "no connection to kiss fc";
      OSD.printP(settings.COLS / 2 - strlen_P(FC_NOT_CONNECTED_STR) / 2, settings.ROWS / 2, FC_NOT_CONNECTED_STR);
      triggerCleanScreen = true;
      fcNotConnectedCount = 0;
      return;
    }

#ifdef IMPULSERC_VTX
    /*if(changevTxTime > 0)
      {
      static const char CHANGE_CHANNELS_STR[] PROGMEM = "changing vtx to ";
      OSD.printP(settings.COLS/2 - (strlen_P(CHANGE_CHANNELS_STR)/2 + 5), 8, CHANGE_CHANNELS_STR);
      OSD.printInt16(settings.COLS/2 - (strlen_P(CHANGE_CHANNELS_STR)/2 + 5) + strlen_P(CHANGE_CHANNELS_STR) + 1, 8, bandSymbols[vTxBand], (int16_t)vTxChannel, 0, 1, "=");
      OSD.printInt16(settings.COLS/2 - (strlen_P(CHANGE_CHANNELS_STR)/2 + 5) + strlen_P(CHANGE_CHANNELS_STR) + 5, 8, (int16_t)pgm_read_word(&vtx_frequencies[settings.m_vTxBand][settings.m_vTxChannel]), 0, 1, "mhz");
      uint8_t timeLeft = (uint8_t)((6000 - (millis() - changevTxTime))/1000);
      OSD.printInt16(settings.COLS/2 - 6, 9, "in ", (int16_t)timeLeft, 0, 1, " seconds");
      if(timeLeft < 1)
      {
        changevTxTime = 0;
        cleanScreen();
      }
      }*/
    vtx_flash_led(1);
#endif

    if (triggerCleanScreen ||
        (abs((DV_PPMs[currentDVItem] + 1000) - (AuxChanVals[settings.m_DVchannel] + 1000)) >= CSettings::DV_PPM_INCREMENT
         && (AuxChanVals[settings.m_DVchannel] + 1000) < (CSettings::DV_PPM_INCREMENT * (CSettings::DISPLAY_DV_SIZE))
         && !moveItems))
    {
      currentDVItem = CSettings::DISPLAY_DV_SIZE - 1;
      while (abs((DV_PPMs[currentDVItem] + 1000) - (AuxChanVals[settings.m_DVchannel] + 1000)) >= CSettings::DV_PPM_INCREMENT && currentDVItem > 0) currentDVItem--;
      triggerCleanScreen = false;
      cleanScreen();
    }

    if (settings.m_tempUnit == 1)
    {
      tempSymbol[0] = fixChar(0xB1);
    }
    else
    {
      tempSymbol[0] = fixChar(0xB0);
    }

    code = inputChecker.ProcessStickInputs(roll, pitch, yaw, armed);

    if (armed == 0 && code & inputChecker.YAW_LONG_LEFT && code & inputChecker.ROLL_LEFT && code & inputChecker.PITCH_DOWN)
    {
      ReviveOSD();
    }

    if (armed == 0 && setupNickname)
    {
      static const char ROLL_UP_DOWN_CHAR_STR[] PROGMEM = "use pitch/roll to set name";
      static const char YAW_LEFT_EXIT_STR[] PROGMEM = "yaw left to exit";
      OSD.printP(settings.COLS / 2 - strlen_P(ROLL_UP_DOWN_CHAR_STR) / 2, settings.ROWS / 2 - 2, ROLL_UP_DOWN_CHAR_STR);
      OSD.printP(settings.COLS / 2 - strlen_P(YAW_LEFT_EXIT_STR) / 2, settings.ROWS / 2 - 1, YAW_LEFT_EXIT_STR);
      if (code & inputChecker.PITCH_UP)
      {
        if (charIndex == 0)
        {
          charIndex = charTableSize - 1;
        }
        else
        {
          charIndex--;
        }
        settingChanged = true;
      }
      if (code & inputChecker.PITCH_DOWN)
      {
        if (charIndex == charTableSize - 1)
        {
          charIndex = 0;
        }
        else
        {
          charIndex++;
        }
        settingChanged = true;
      }
      if (code & inputChecker.ROLL_LEFT && charSelected > 0)
      {
        charSelected--;
        charIndex = findCharPos(settings.m_nickname[charSelected]);
      }
      if (code & inputChecker.ROLL_RIGHT && charSelected < CSettings::NICKNAME_STR_SIZE - 2)
      {
        charSelected++;
        charIndex = findCharPos(settings.m_nickname[charSelected]);
      }
      settings.m_nickname[charSelected] = charTable[charIndex];
      OSD.setCursor(settings.COLS / 2 - CSettings::NICKNAME_STR_SIZE / 2, settings.ROWS / 2);
      for (i = 0; i < CSettings::NICKNAME_STR_SIZE - 1; i++)
      {
        if (i == charSelected && timer1sec)
        {
          OSD.print(fixChar(' '));
        }
        else
        {
          if (settings.m_nickname[i] == 0x00 && i == 0)
          {
            OSD.print(fixChar(charTable[charIndex]));
          }
          else
          {
            OSD.print(fixChar(settings.m_nickname[i]));
          }
        }
      }
      if (code & inputChecker.YAW_LEFT)
      {
        uint8_t nickEnd = 7;
        while ((settings.m_nickname[nickEnd] == 0x00 || settings.m_nickname[nickEnd] == 0x20) && nickEnd > 0) nickEnd--;
        settings.m_nickname[nickEnd + 1] = 0x00;
        const char mustache[] = { 0x7F, 0x80, 0x81, 0x82, 0x00 };
        const char secret[] = { 0x35, 0x37, 0x33, 0x33, 0x31, 0x33, 0x00 }; //573313
        if(strcmp(settings.m_nickname, secret) == 0) strcpy(settings.m_nickname, mustache);
        setupNickname = false;
        cleanScreen();
      }
      else
      {
        return;
      }
    }

    if (armed == 0 && moveItems)
    {
      if (startMoveTime == 0) startMoveTime = millis();
      static const char ROLL_UP_DOWN_MOVE_STR[] PROGMEM = "use pitch/roll to move";
      static const char YAW_LEFT_SELECT_STR[] PROGMEM = "yaw left for next item";
      static const char YAW_LONG_LEFT_EXIT_STR[] PROGMEM = "yaw long left to exit";
      if (millis() - startMoveTime < 5000)
      {
        OSD.printP(settings.COLS / 2 - strlen_P(ROLL_UP_DOWN_MOVE_STR) / 2, settings.ROWS / 2 - 1, ROLL_UP_DOWN_MOVE_STR);
        OSD.printP(settings.COLS / 2 - strlen_P(YAW_LEFT_SELECT_STR) / 2, settings.ROWS / 2, YAW_LEFT_SELECT_STR);
        OSD.printP(settings.COLS / 2 - strlen_P(YAW_LONG_LEFT_EXIT_STR) / 2, settings.ROWS / 2 + 1, YAW_LONG_LEFT_EXIT_STR);
      }
      if (code & inputChecker.YAW_LEFT)
      {
        OSD_ITEM_BLINK[moveSelected] = false;
        if (moveSelected == CSettings::OSD_ITEMS_SIZE - 1)
        {
          moveSelected = 0;
        }
        else
        {
          moveSelected++;
        }
        OSD_ITEM_BLINK[moveSelected] = true;
      }
      uint8_t moveCount = 1;
      if (moveSelected > STOPW)
      {
        moveSelected = STOPW + (moveSelected - STOPW - 1) * 3 + 1;
        moveCount = 3;
      }
      for (i = 0; i < moveCount; i++)
      {
        if (code & inputChecker.PITCH_UP)
        {
          if (settings.m_OSDItems[moveSelected][1] > 0)
          {
            settings.m_OSDItems[moveSelected][1]--;
            cleanScreen();
            settingChanged = true;
          }
        }
        if (code & inputChecker.PITCH_DOWN)
        {
          if (settings.m_OSDItems[moveSelected][1] < settings.ROWS - 1)
          {
            settings.m_OSDItems[moveSelected][1]++;
            cleanScreen();
            settingChanged = true;
          }
        }
        if (code & inputChecker.ROLL_LEFT)
        {
          if (settings.m_OSDItems[moveSelected][0] > 0)
          {
            settings.m_OSDItems[moveSelected][0]--;
            cleanScreen();
            settingChanged = true;
          }
        }
        if (code & inputChecker.ROLL_RIGHT)
        {
          if (settings.m_OSDItems[moveSelected][0] < settings.COLS - 1 - settings.m_goggle && itemLengthOK[moveSelected] == 0)
          {
            settings.m_OSDItems[moveSelected][0]++;
            cleanScreen();
            settingChanged = true;
          }
        }
        moveSelected++;
      }
      if ((moveSelected - 1) > STOPW)
      {
        moveSelected = STOPW + (moveSelected - STOPW - 1) / 3;
      }
      else
      {
        moveSelected--;
      }
      if (code & inputChecker.YAW_LONG_LEFT)
      {
        OSD_ITEM_BLINK[moveSelected] = false;
        settings.SetupPPMs(DV_PPMs);
        moveItems = false;
        menuActive = true;
        startMoveTime = 0;
        resetBlinking();
        cleanScreen();
      }
    }

    if (updateFontComplete)
    {
      delay(3000);
      updateFontComplete = false;
      cleanScreen();
    }

    if (updateFont)
    {
      delay(3000);
      OSD.updateFont();
      updateFont = false;
      setupMAX7456();
      cleanScreen();
      static const char FONT_COMPLETE_STR[] PROGMEM = "font updated";
      OSD.printP(settings.COLS / 2 - strlen_P(FONT_COMPLETE_STR) / 2, settings.ROWS / 2, FONT_COMPLETE_STR);
      updateFontComplete = true;
      return;
    }

    if (shiftOSDactive && armed == 0)
    {
      static const char OSD_SHIFT_STR[] PROGMEM = "roll/pitch to center osd";
      OSD.printP(settings.COLS / 2 - strlen_P(OSD_SHIFT_STR) / 2, settings.ROWS / 2 - 1, OSD_SHIFT_STR);
      static const char OSD_SHIFT_EXIT_STR[] PROGMEM = "yaw left to exit";
      OSD.printP(settings.COLS / 2 - strlen_P(OSD_SHIFT_EXIT_STR) / 2, settings.ROWS / 2, OSD_SHIFT_EXIT_STR);

      boolean changedOffset = false;
      if (code & inputChecker.ROLL_RIGHT)
      {
        settings.m_xOffset++;
        changedOffset = true;
      }
      if (code & inputChecker.ROLL_LEFT)
      {
        settings.m_xOffset--;
        changedOffset = true;
      }
      if (code & inputChecker.PITCH_UP)
      {
        settings.m_yOffset--;
        changedOffset = true;
      }
      if (code & inputChecker.PITCH_DOWN)
      {
        settings.m_yOffset++;
        changedOffset = true;
      }
      settingChanged |= changedOffset;
      if (changedOffset)
      {
        cleanScreen();
        OSD.setTextOffset(settings.m_xOffset, settings.m_yOffset);
      }
      if (code & inputChecker.YAW_LEFT)
      {
        shiftOSDactive = false;
        menuActive = true;
        cleanScreen();
      }
    }

    if (armed == 0 && batterySelect)
    {
      if (!showBat)
      {
        cleanScreen();
        showBat = true;
      }
      if ((code & inputChecker.ROLL_LEFT) && settings.m_activeBattery > 0)
      {
        settings.m_activeBattery--;
        settings.FixBatWarning();
        settingChanged = true;
      }
      if ((code & inputChecker.ROLL_RIGHT) && settings.m_activeBattery < 3)
      {
        settings.m_activeBattery++;
        settings.FixBatWarning();
        settingChanged = true;
      }
      if ((code & inputChecker.PITCH_UP) && settings.m_batMAH[settings.m_activeBattery] < 32000)
      {
        settings.m_batMAH[settings.m_activeBattery] += BAT_MAH_INCREMENT;
        settings.FixBatWarning();
        settingChanged = true;
      }
      if ((code & inputChecker.PITCH_DOWN) && settings.m_batMAH[settings.m_activeBattery] > 300)
      {
        settings.m_batMAH[settings.m_activeBattery] -= BAT_MAH_INCREMENT;
        settings.FixBatWarning();
        settingChanged = true;
      }
      static const char BATTERY_STR[] PROGMEM = "battery ";
      uint8_t batCol = settings.COLS / 2 - (strlen_P(BATTERY_STR) + 1) / 2;
      OSD.printInt16P(batCol, settings.ROWS / 2 - 1, BATTERY_STR, settings.m_activeBattery + 1, 0, 1);
      batCol = settings.COLS / 2 - 3;
      OSD.printInt16(batCol, settings.ROWS / 2, settings.m_batMAH[settings.m_activeBattery], 0, 1, "mah", 2);

      static const char WARN_STR[] PROGMEM = "warn at ";
      batCol = settings.COLS / 2 - (strlen_P(WARN_STR) + 6) / 2;
      OSD.printInt16P(batCol, settings.ROWS / 2 + 1, WARN_STR, settings.m_batWarningMAH, 0, 1, "mah", 2);

      if (batterySelect)
      {
        static const char YAW_LEFT_STR[] PROGMEM = "yaw left to go back";
        OSD.printP(settings.COLS / 2 - strlen_P(YAW_LEFT_STR) / 2, settings.ROWS / 2 + 2, YAW_LEFT_STR);
      }
      if (batterySelect && yaw < 250)
      {
        batterySelect = false;
      }
      return;
    }
    else
    {
      if (showBat)
      {
        cleanScreen();
        showBat = false;
      }
    }

    if (saveSettings && settingChanged && !shiftOSDactive)
    {
      settings.WriteSettings();
      settingChanged = false;
      saveSettings = false;
      static const char SETTINGS_SAVED_STR[] PROGMEM = "settings saved";
      OSD.printP(settings.COLS / 2 - strlen_P(SETTINGS_SAVED_STR) / 2, settings.ROWS / 2, SETTINGS_SAVED_STR);
      delay(1500);
      cleanScreen();
    }

    if (armed == 0 && ((code &  inputChecker.YAW_LONG_LEFT) || menuActive))
    {
      if (!menuActive)
      {
        menuActive = true;
        cleanScreen();
        activePage = (void*)MainMenu;
      }
      fptr temp = (fptr) activePage;
      activePage = (void*)temp();
      return;
    }
    else
    {
      if (menuWasActive)
      {
        menuWasActive = false;
        activeMenuItem = 0;
        cleanScreen();
      }
    }

    if (armed == 0 && armedOnce && last_Aux_Val != AuxChanVals[settings.m_DVchannel])
    {
      DV_change_time = millis();
      last_Aux_Val = AuxChanVals[settings.m_DVchannel];
    }

    uint8_t TMPmargin          = 0;
    uint8_t CurrentMargin      = 0;
    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_RC_THROTTLE])
    {
      if (OSD_ITEM_BLINK[THROTTLE]) OSD.blink1sec();
      itemLengthOK[THROTTLE] = OSD.printInt16( settings.m_OSDItems[THROTTLE][0], settings.m_OSDItems[THROTTLE][1], throttle, 0, 1, "%", 2, THROTTLEp);
      //ESCmarginTop = 1;
    }

    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_NICKNAME])
    {
      uint8_t nickBlanks = 0;
      itemLengthOK[NICKNAME] = OSD.checkPrintLength(settings.m_OSDItems[NICKNAME][0], settings.m_OSDItems[NICKNAME][1], (uint8_t)strlen(settings.m_nickname), nickBlanks, NICKNAMEp);
      if (OSD_ITEM_BLINK[NICKNAME] && timer1sec)
      {
        OSD.printSpaces(strlen(settings.m_nickname));
      }
      else
      {
        OSD.print( fixStr(settings.m_nickname) );
      }
    }

    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_COMB_CURRENT])
    {
      if(settings.m_beerMug == 0)
      {
        if (OSD_ITEM_BLINK[AMPS]) OSD.blink1sec();
        itemLengthOK[AMPS] = OSD.printInt16(settings.m_OSDItems[AMPS][0], settings.m_OSDItems[AMPS][1], current, 1, 0, "a", 2, AMPSp);
      }
      else
      {
        static const char beerMug1[] = { 0x01, 0x02, 0x00 }; 
        static const char beerMug2[] = { 0x09, 0x0A, 0x00 };
        int8_t beerRow = settings.m_OSDItems[AMPS][1]-1;
        if(beerRow < 0) beerRow = 0;
        uint8_t ampBlanks = 0;
        itemLengthOK[AMPS] = OSD.checkPrintLength(settings.m_OSDItems[AMPS][0], beerRow, (uint8_t)strlen(beerMug1), ampBlanks, AMPSp);
        if (OSD_ITEM_BLINK[AMPS] && timer1sec)
        {
          OSD.printSpaces(2);
          OSD.checkPrintLength(settings.m_OSDItems[AMPS][0], beerRow+1, (uint8_t)strlen(beerMug1), ampBlanks, AMPSp);
          OSD.printSpaces(2);
        }
        else
        {
          OSD.print(beerMug1);
          OSD.checkPrintLength(settings.m_OSDItems[AMPS][0], beerRow+1, (uint8_t)strlen(beerMug1), ampBlanks, AMPSp);
          OSD.print(beerMug2);
        }
      }
    }

    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_LIPO_VOLTAGE])
    {
      if (OSD_ITEM_BLINK[VOLTAGE]) OSD.blink1sec();
      itemLengthOK[VOLTAGE] = OSD.printInt16( settings.m_OSDItems[VOLTAGE][0], settings.m_OSDItems[VOLTAGE][1], LipoVoltage / 10, 1, 1, "v", 1, VOLTAGEp);
    }

    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_MA_CONSUMPTION])
    {
      if (settings.m_displaySymbols == 1)
      {
        uint8_t batCount = (LipoMAH + previousMAH) / settings.m_batSlice;
        uint8_t batStatus = 0xEC;
        while (batCount > 4)
        {
          batStatus--;
          batCount--;
        }
        batterySymbol[2] = (char)batStatus;
        batStatus = 0xEC;
        while (batCount > 0)
        {
          batStatus--;
          batCount--;
        }
        batterySymbol[1] = (char)batStatus;
        uint8_t mahBlanks = 0;
        itemLengthOK[MAH] = OSD.checkPrintLength(settings.m_OSDItems[MAH][0], settings.m_OSDItems[MAH][1], 4, mahBlanks, MAHp);
        if (OSD_ITEM_BLINK[MAH] && timer1sec)
        {
          OSD.printSpaces(4);
        }
        else
        {
          OSD.print(batterySymbol);
        }
      }
      else
      {
        if (OSD_ITEM_BLINK[MAH]) OSD.blink1sec();
        itemLengthOK[MAH] = OSD.printInt16(settings.m_OSDItems[MAH][0], settings.m_OSDItems[MAH][1], LipoMAH + previousMAH, 0, 1, "mah", MAHp);
      }
    }

    if (settings.m_displaySymbols == 1)
    {
      ESCSymbol[0] = fixChar((char)0x7E);
    }
    else
    {
      ESCSymbol[0] = 0x00;
    }

    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_ESC_KRPM])
    {
      static char KR[4];
      if (settings.m_displaySymbols == 1)
      {
        for (i = 0; i < 4; i++)
        {
          if (millis() - krTime[i] > (10000 / motorKERPM[i]))
          {
            krTime[i] = millis();
            if ((i + 1) % 2 == 0)
            {
              krSymbol[i]--;
            }
            else
            {
              krSymbol[i]++;
            }
            if (krSymbol[i] > 0xA3)
            {
              krSymbol[i] = 0x9C;
            }
            if (krSymbol[i] < 0x9C)
            {
              krSymbol[i] = 0xA3;
            }
          }
          KR[i] = (char)krSymbol[i];
        }
        uint8_t ESCkrBlanks = 0;
        itemLengthOK[ESC1kr] = OSD.checkPrintLength(settings.m_OSDItems[ESC1kr][0], settings.m_OSDItems[ESC1kr][1], 1, ESCkrBlanks, ESC1kr);
        if (OSD_ITEM_BLINK[ESC1] && timer1sec) OSD.print(' ');
        else OSD.print(KR[0]);
        itemLengthOK[ESC2kr] = OSD.checkPrintLength(settings.m_OSDItems[ESC2kr][0], settings.m_OSDItems[ESC2kr][1], 1, ESCkrBlanks, ESC2kr);
        if (OSD_ITEM_BLINK[ESC2] && timer1sec) OSD.print(' ');
        else OSD.print(KR[1]);
        itemLengthOK[ESC3kr] = OSD.checkPrintLength(settings.m_OSDItems[ESC3kr][0], settings.m_OSDItems[ESC3kr][1], 1, ESCkrBlanks, ESC3kr);
        if (OSD_ITEM_BLINK[ESC3] && timer1sec) OSD.print(' ');
        else OSD.print(KR[2]);
        itemLengthOK[ESC4kr] = OSD.checkPrintLength(settings.m_OSDItems[ESC4kr][0], settings.m_OSDItems[ESC4kr][1], 1, ESCkrBlanks, ESC4kr);
        if (OSD_ITEM_BLINK[ESC4] && timer1sec) OSD.print(' ');
        else OSD.print(KR[3]);
      }
      else
      {
        static char KR2[3];
        KR2[0] = 'k';
        KR2[1] = 'r';
        KR2[2] = 0x00;
        if (OSD_ITEM_BLINK[ESC1]) OSD.blink1sec();
        itemLengthOK[ESC1kr] = OSD.printInt16(settings.m_OSDItems[ESC1kr][0], settings.m_OSDItems[ESC1kr][1], motorKERPM[0], 1, 1, KR2, 1, ESC1kr);
        if (OSD_ITEM_BLINK[ESC2]) OSD.blink1sec();
        itemLengthOK[ESC2kr] = OSD.printInt16(settings.m_OSDItems[ESC2kr][0], settings.m_OSDItems[ESC2kr][1], motorKERPM[1], 1, 0, KR2, 1, ESC2kr);
        if (OSD_ITEM_BLINK[ESC3]) OSD.blink1sec();
        itemLengthOK[ESC3kr] = OSD.printInt16(settings.m_OSDItems[ESC3kr][0], settings.m_OSDItems[ESC3kr][1], motorKERPM[2], 1, 0, KR2, 1, ESC3kr);
        if (OSD_ITEM_BLINK[ESC4]) OSD.blink1sec();
        itemLengthOK[ESC4kr] = OSD.printInt16( settings.m_OSDItems[ESC4kr][0], settings.m_OSDItems[ESC4kr][1], motorKERPM[3], 1, 1, KR2, 1, ESC4kr);
      }

      TMPmargin++;
      CurrentMargin++;
    }

    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_ESC_CURRENT])
    {
      if (OSD_ITEM_BLINK[ESC1]) OSD.blink1sec();
      itemLengthOK[ESC1voltage] = OSD.printInt16(settings.m_OSDItems[ESC1voltage][0], settings.m_OSDItems[ESC1voltage][1] + CurrentMargin, motorCurrent[0], 2, 1, "a", 1, ESC1voltage, ESCSymbol);
      if (OSD_ITEM_BLINK[ESC2]) OSD.blink1sec();
      itemLengthOK[ESC2voltage] = OSD.printInt16(settings.m_OSDItems[ESC2voltage][0], settings.m_OSDItems[ESC2voltage][1] + CurrentMargin, motorCurrent[1], 2, 0, "a", 1, ESC2voltage, ESCSymbol);
      if (OSD_ITEM_BLINK[ESC3]) OSD.blink1sec();
      itemLengthOK[ESC3voltage] = OSD.printInt16(settings.m_OSDItems[ESC3voltage][0], settings.m_OSDItems[ESC3voltage][1] - CurrentMargin, motorCurrent[2], 2, 0, "a", 1, ESC3voltage, ESCSymbol);
      if (OSD_ITEM_BLINK[ESC4]) OSD.blink1sec();
      itemLengthOK[ESC4voltage] = OSD.printInt16(settings.m_OSDItems[ESC4voltage][0], settings.m_OSDItems[ESC4voltage][1] - CurrentMargin, motorCurrent[3], 2 , 1, "a", 1, ESC4voltage, ESCSymbol);
      TMPmargin++;
    }

    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_ESC_TEMPERATURE])
    {
      if (OSD_ITEM_BLINK[ESC1]) OSD.blink1sec();
      itemLengthOK[ESC1temp] = OSD.printInt16( settings.m_OSDItems[ESC1temp][0], settings.m_OSDItems[ESC1temp][1] + TMPmargin, ESCTemps[0], 0, 1, tempSymbol, 1, ESC1temp, ESCSymbol);
      if (OSD_ITEM_BLINK[ESC2]) OSD.blink1sec();
      itemLengthOK[ESC2temp] = OSD.printInt16(settings.m_OSDItems[ESC2temp][0], settings.m_OSDItems[ESC2temp][1] + TMPmargin, ESCTemps[1], 0, 0, tempSymbol, 1, ESC2temp, ESCSymbol);
      if (OSD_ITEM_BLINK[ESC3]) OSD.blink1sec();
      itemLengthOK[ESC3temp] = OSD.printInt16(settings.m_OSDItems[ESC3temp][0], settings.m_OSDItems[ESC3temp][1] - TMPmargin, ESCTemps[2], 0, 0, tempSymbol, 1, ESC3temp, ESCSymbol);
      if (OSD_ITEM_BLINK[ESC4]) OSD.blink1sec();
      itemLengthOK[ESC4temp] = OSD.printInt16(settings.m_OSDItems[ESC4temp][0], settings.m_OSDItems[ESC4temp][1] - TMPmargin, ESCTemps[3], 0, 1, tempSymbol, 1, ESC4temp, ESCSymbol);
    }

    if (AuxChanVals[settings.m_DVchannel] > DV_PPMs[DISPLAY_TIMER])
    {
      char stopWatchStr[] = { 0x00, 0x00 };
      if (settings.m_displaySymbols == 1)
      {
        if (time - old_time > 1000)
        {
          stopWatch++;
          old_time = time;
        }
        if (stopWatch > 0x9B)
        {
          stopWatch = 0x94;
        }
        stopWatchStr[0] = stopWatch;
      }
      else
      {
        stopWatchStr[0] = 0x00;
      }
      if (OSD_ITEM_BLINK[STOPW]) OSD.blink1sec();
      OSD.printTime(settings.m_OSDItems[STOPW][0], settings.m_OSDItems[STOPW][1], time, stopWatchStr, STOPWp);
    }

    if (DV_change_time > 0 && (millis() - DV_change_time) > 3000 && last_Aux_Val == AuxChanVals[settings.m_DVchannel])
    {
      DV_change_time = 0;
      cleanScreen();
    }

    if (!correctItemsOnce && moveItems)
    {
      for (i = 0; i < CSettings::OSD_ITEMS_POS_SIZE; i++)
      {
        if (itemLengthOK[i] > 0)
        {
          settings.m_OSDItems[i][0] = itemLengthOK[i];
        }
      }
      correctItemsOnce = true;
    }

    if (symbolOnOffChanged)
    {
      settings.fixColBorders();
      symbolOnOffChanged = false;
    }
  }
}

#ifdef DEBUG
int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif
