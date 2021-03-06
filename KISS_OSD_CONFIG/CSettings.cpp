#include "CSettings.h"
#include <EEPROM.h>
#include "Config.h"
#include "fixFont.h"

CSettings::CSettings()
{
  COLS = 28;
  m_maxWatts = 2500;
  LoadDefaults();
}

bool CSettings::cleanEEPROM()
{
  int i;
  bool cleaned = true;
  byte check = 0xDD;
  for(i=(EEPROM.length()-1); i>(EEPROM.length()-5); i--)
  {
    byte readB = EEPROM.read(i);    
    if(readB != check) cleaned = false;
  }
  if(!cleaned)
  {
    for(i=0; i<EEPROM.length(); i++) EEPROM.update(i,0);
    for(i=(EEPROM.length()-1); i>(EEPROM.length()-5); i--)
    {
      EEPROM.update(i, check);
    }
  }
  return cleaned;
}

void CSettings::LoadDefaults()
{
  m_batWarning = 1; //on by default
  m_batMAH[0] = 1300; //1300 mAh by default
  m_batMAH[1] = 1500; //1500 mAh by default
  m_batMAH[2] = 1800; //1800 mAh by default
  m_batMAH[3] = 2250; //2250 mAh by default
  m_activeBattery = 0;
  m_batWarningPercent = 23; //23% by default
  FixBatWarning();
  m_DVchannel = 0; //AUX1 default
  m_tempUnit = 0; //°C default
  m_lastMAH = 0;
  m_fontSize = 1;
  m_displaySymbols = 1;
  m_vTxChannel = 4; // Raceband 5 @ 25mW default
  m_vTxBand = 4;
  m_vTxPower = 0;
  m_xOffset = 0; // Center OSD offsets
  m_yOffset = 0;
  m_stats = 1;
  m_DISPLAY_DV[DISPLAY_NICKNAME] = 8;
  m_DISPLAY_DV[DISPLAY_TIMER] = 2;
  m_DISPLAY_DV[DISPLAY_RC_THROTTLE] = 7;
  m_DISPLAY_DV[DISPLAY_COMB_CURRENT] = 4;
  m_DISPLAY_DV[DISPLAY_LIPO_VOLTAGE] = 0;
  m_DISPLAY_DV[DISPLAY_MA_CONSUMPTION] = 1;
  m_DISPLAY_DV[DISPLAY_ESC_KRPM] = 3;
  m_DISPLAY_DV[DISPLAY_ESC_CURRENT] = 5;
  m_DISPLAY_DV[DISPLAY_ESC_TEMPERATURE] = 6;
  uint8_t i;
  for(i=0; i < NICKNAME_STR_SIZE; i++)
  {
    m_nickname[i] = 0x00;
  }
  m_OSDItems[ESC1kr][0] = 0;
  m_OSDItems[ESC1kr][1] = 1;
  m_OSDItems[ESC1voltage][0] = 0;
  m_OSDItems[ESC1voltage][1] = 1;
  m_OSDItems[ESC1temp][0] = 0;
  m_OSDItems[ESC1temp][1] = 1;
  m_OSDItems[ESC2kr][0] = COLS-1;
  m_OSDItems[ESC2kr][1] = 1;
  m_OSDItems[ESC2voltage][0] = COLS;
  m_OSDItems[ESC2voltage][1] = 1;
  m_OSDItems[ESC2temp][0] = COLS;
  m_OSDItems[ESC2temp][1] = 1;
  m_OSDItems[ESC3kr][0] = COLS-1;
  m_OSDItems[ESC3kr][1] = ROWS-2;
  m_OSDItems[ESC3voltage][0] = COLS;
  m_OSDItems[ESC3voltage][1] = ROWS-2;
  m_OSDItems[ESC3temp][0] = COLS;
  m_OSDItems[ESC3temp][1] = ROWS-2;
  m_OSDItems[ESC4kr][0] = 0;
  m_OSDItems[ESC4kr][1] = ROWS-2;
  m_OSDItems[ESC4voltage][0] = 0;
  m_OSDItems[ESC4voltage][1] = ROWS-2;
  m_OSDItems[ESC4temp][0] = 0;
  m_OSDItems[ESC4temp][1] = ROWS-2;
  m_OSDItems[VOLTAGE][0] = 0;
  m_OSDItems[VOLTAGE][1] = ROWS-1;
  m_OSDItems[AMPS][0] = COLS;
  m_OSDItems[AMPS][1] = 0;
  m_OSDItems[THROTTLE][0] = 0;
  m_OSDItems[THROTTLE][1] = 0;
  m_OSDItems[STOPW][0] = COLS/2 - 3;
  m_OSDItems[STOPW][1] = ROWS - 2;
  m_OSDItems[NICKNAME][0] = COLS/2 - 4;
  m_OSDItems[NICKNAME][1] = ROWS - 1;
  m_OSDItems[MAH][0] = COLS - 1;
  m_OSDItems[MAH][1] = ROWS - 1;
  m_goggle = 0; //0 = fatshark, 1 = headplay
  m_beerMug = 1;
  m_Moustache = 1;
  m_voltWarning = 0;
  m_minVolts = 148;
}

void CSettings::fixColBorders()
{
  int8_t moveDir = 1;
  if(m_displaySymbols == 1) moveDir = -1;
  uint8_t i;
  for(i=0; i < OSD_ITEMS_POS_SIZE; i++)
  {
    if(m_colBorder[i])
    {
      if(i== ESC1kr || i== ESC2kr || i== ESC3kr || i== ESC4kr)
      {
        m_OSDItems[i][0] += (int8_t)((int8_t)5 * moveDir);
      }
      else
      {
        if(i >= STOPWp)
        {
           m_OSDItems[i][0] += (int8_t)((int8_t)1 * moveDir);
        }
      }      
    }
  }
}

int16_t CSettings::ReadInt16_t(byte lsbPos, byte msbPos)
{
  byte msb, lsb;
  
  lsb = EEPROM.read(lsbPos);
  msb = EEPROM.read(msbPos);
  
  int16_t value = msb;
  value = value << 8;
  value |= lsb;
  return value;
}

void CSettings::ReadSettingsInternal()
{
  m_batWarningMAH = ReadInt16_t(0x03,0x04);
  m_batWarning = EEPROM.read(0x05);
  m_activeBattery = EEPROM.read(0x06);
  m_DVchannel = EEPROM.read(0x07);
  uint8_t i;
  byte pos = 0x08;
  for(i=0; i < 4; i++)
  {
    m_batMAH[i] = ReadInt16_t(pos, pos+1);
    pos += 2;
  }
  m_batWarningPercent = EEPROM.read(pos);
  pos++;
  m_tempUnit = EEPROM.read(pos);
  pos++;
  m_fontSize = EEPROM.read(pos);
  pos++;
  m_displaySymbols = EEPROM.read(pos);
  pos++;
  m_vTxChannel = EEPROM.read(pos);
  pos++;
  m_vTxBand = EEPROM.read(pos);
  pos++;
  m_vTxPower = EEPROM.read(pos);
  pos++;
  m_xOffset = EEPROM.read(pos);
  pos++;
  m_yOffset = EEPROM.read(pos);
  pos++;
  for(i=0; i<DISPLAY_DV_SIZE; i++)
  {
    m_DISPLAY_DV[i] = EEPROM.read(pos);
    pos++;
  }
  for(i=0; i<NICKNAME_STR_SIZE; i++)
  {
    m_nickname[i] = EEPROM.read(pos);
    pos++;
  }
  uint8_t j;
  for(i=0; i < OSD_ITEMS_POS_SIZE; i++)
  {
    for(j=0; j<2; j++)
    {
      m_OSDItems[i][j] = EEPROM.read(pos);
      pos++;
    }
  }
  m_goggle = EEPROM.read(pos);
  pos++;
  m_beerMug =  EEPROM.read(pos);
  pos++;
  m_Moustache = EEPROM.read(pos);
  pos++;
  m_maxWatts = ReadInt16_t(pos, pos+1);
  pos += 2;
  m_voltWarning = EEPROM.read(pos);
  pos++;
  m_minVolts = EEPROM.read(pos);
  pos++;
  
  m_lastMAH = ReadInt16_t(200, 201);
}

void CSettings::UpgradeFromPreviousVersion(uint8_t ver)
{
  if(ver >= 0x09)
  {
    ReadSettingsInternal();
    if(ver < 0x0A)
    {
      m_beerMug = 1;
      m_Moustache = 1;
      m_maxWatts = 2500;
    }
    if(ver < 0x0B)
    {
      m_voltWarning = 0;
      m_minVolts = 148;
    } 
  }
}

void CSettings::ReadSettings()
{
  uint8_t settingsVer = EEPROM.read(0x01);
  if(settingsVer < 0x0B) //first start of OSD - or older version
  {
    UpgradeFromPreviousVersion(settingsVer);
    WriteSettings(); //write defaults
    EEPROM.update(0x01,0x0B);
  }
  else
  {
    ReadSettingsInternal();
  }
  FixBatWarning();
}

void CSettings::WriteInt16_t(byte lsbPos, byte msbPos, int16_t value)
{
  byte msb, lsb;
  lsb = (byte)(value & 0x00FF);
  msb = (byte)((value & 0xFF00) >> 8);
  
  EEPROM.update(lsbPos, lsb); // LSB
  EEPROM.update(msbPos, msb); // MSB
}

void CSettings::WriteSettings()
{
  WriteInt16_t(0x03,0x04,m_batWarningMAH);
  EEPROM.update(0x05,(byte)m_batWarning);
  EEPROM.update(0x06,(byte)m_activeBattery);
  EEPROM.update(0x07,(byte)m_DVchannel);
  uint8_t i;
  byte pos = 0x08;
  for(i=0; i < 4; i++)
  {
    WriteInt16_t(pos, pos+1, m_batMAH[i]);
    pos += 2;
  }
  EEPROM.update(pos, (byte)m_batWarningPercent); 
  pos++;
  EEPROM.update(pos, (byte)m_tempUnit);
  pos++;
  EEPROM.update(pos, (byte)m_fontSize);
  pos++;
  EEPROM.update(pos, (byte)m_displaySymbols);
  pos++;
  EEPROM.update(pos, (byte)m_vTxChannel);
  pos++;
  EEPROM.update(pos, (byte)m_vTxBand);
  pos++;
  EEPROM.update(pos, (byte)m_vTxPower);
  pos++;
  EEPROM.update(pos, (byte)m_xOffset);
  pos++;
  EEPROM.update(pos, (byte)m_yOffset);
  pos++;
  for(i=0; i<DISPLAY_DV_SIZE; i++)
  {
    EEPROM.update(pos, (byte)m_DISPLAY_DV[i]);
    pos++;
  }
  for(i=0; i<NICKNAME_STR_SIZE; i++)
  {
    EEPROM.update(pos, (byte)m_nickname[i]);
    pos++;
  }
  uint8_t j;
  for(i=0; i < OSD_ITEMS_POS_SIZE; i++)
  {
    for(j=0; j<2; j++)
    {
      EEPROM.update(pos, (byte)m_OSDItems[i][j]);
      pos++;
    }
  }
  EEPROM.update(pos, (byte)m_goggle);
  pos++;
  EEPROM.update(pos, (byte)m_beerMug);
  pos++;
  EEPROM.update(pos, (byte)m_Moustache);
  pos++;
  WriteInt16_t(pos, pos+1, m_maxWatts);
  pos += 2;
  EEPROM.update(pos, (byte)m_voltWarning);
  pos++;
  EEPROM.update(pos, (byte)m_minVolts);
  pos++;
}

void CSettings::FixBatWarning()
{
  uint32_t temp = ((uint32_t)m_batMAH[m_activeBattery] * (uint32_t)m_batWarningPercent) / (uint32_t)100;
  m_batWarningMAH = m_batMAH[m_activeBattery] - (int16_t)temp;
  m_batSlice = m_batMAH[m_activeBattery] / 8;
}

void CSettings::WriteLastMAH()
{
  WriteInt16_t(200, 201, m_lastMAH);
}

void CSettings::SetupPPMs(int16_t *dv_ppms, bool all)
{
  uint8_t i;
  if(all)
  {
    for(i=0; i<DISPLAY_DV_SIZE; i++)
    {
      dv_ppms[i] = -1001;      
    }
  }
  else
  {
    for(i=0; i<DISPLAY_DV_SIZE; i++)
    {
      dv_ppms[i] = -1000 + (m_DISPLAY_DV[i] * DV_PPM_INCREMENT);      
    }
  }
}

void CSettings::UpdateMaxWatt(int16_t maxWatt)
{
  m_maxWatts = maxWatt;
  WriteInt16_t(202, 203, m_maxWatts);
}

