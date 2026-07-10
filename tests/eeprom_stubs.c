#include <stdint.h>
#include <string.h>

#define TEST_EE_SIZE (16u * 1024u)

static uint8_t s_eeprom[TEST_EE_SIZE];

uint8_t ee_CheckOk(void)
{
    return 1u;
}

uint8_t ee_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize)
{
    if ((uint32_t)_usAddress + (uint32_t)_usSize > TEST_EE_SIZE) {
        return 0u;
    }
    memcpy(_pReadBuf, &s_eeprom[_usAddress], _usSize);
    return 1u;
}

uint8_t ee_WriteBytes(uint8_t *_pWriteBuf, uint16_t _usAddress, uint16_t _usSize)
{
    if ((uint32_t)_usAddress + (uint32_t)_usSize > TEST_EE_SIZE) {
        return 0u;
    }
    memcpy(&s_eeprom[_usAddress], _pWriteBuf, _usSize);
    return 1u;
}
