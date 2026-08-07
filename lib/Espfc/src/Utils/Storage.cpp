#ifndef UNIT_TEST

#include "Utils/Storage.h"
#include "ModelConfig.h"
#include <Arduino.h>
#include <EEPROM.h>

#if defined(NO_GLOBAL_INSTANCES) || defined(NO_GLOBAL_EEPROM)
static EEPROMClass EEPROM;
#endif

namespace Espfc::Utils {

int Storage::begin()
{
  EEPROM.begin(EEPROM_SIZE);
  static_assert(sizeof(ModelConfig) <= EEPROM_SIZE, "ModelConfig Size too big");
  return 1;
}

StorageResult Storage::load(ModelConfig& config) const
{
  // return STORAGE_ERR_BAD_MAGIC;

  int addr = 0;
  uint8_t magic = EEPROM.read(addr++);
  if (EEPROM_MAGIC != magic)
  {
    return STORAGE_ERR_BAD_MAGIC;
  }

  uint8_t version = EEPROM.read(addr++);
  if (version > EEPROM_VERSION)
  {
    return STORAGE_ERR_BAD_VERSION; // saved by a newer firmware
  }

  uint16_t size = 0;
  size = EEPROM.read(addr++);
  size |= EEPROM.read(addr++) << 8;
  if (size > sizeof(ModelConfig))
  {
    return STORAGE_ERR_BAD_SIZE;
  }

  ModelConfig def;
  uint8_t* buf = reinterpret_cast<uint8_t*>(&def);
  for (uint16_t i = 0; i < size; i++)
  {
    buf[i] = EEPROM.read(addr + i);
  }
  if (version < EEPROM_VERSION)
  {
    // fields appended to ModelConfig after the persisted version
    def.airModeActivateThrottle = 1400;
  }
  config = def;
  return STORAGE_LOAD_SUCCESS;
}

StorageResult Storage::save(const ModelConfig& config)
{
  int addr = 0;
  uint16_t size = sizeof(ModelConfig);
  EEPROM.write(addr++, EEPROM_MAGIC);
  EEPROM.write(addr++, EEPROM_VERSION);
  EEPROM.write(addr++, size & 0xFF);
  EEPROM.write(addr++, (size >> 8) & 0xFF);
  EEPROM.put(addr, config);
  bool ok = EEPROM.commit();
  if (!ok) return STORAGE_SAVE_ERROR;
  return STORAGE_SAVE_SUCCESS;
}

} // namespace Espfc::Utils

#endif
