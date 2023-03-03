#include <stdint.h>
#include "ble_stubs.h"
#include "FunctionLib.h"

void BOARD_GetMCUUid(uint8_t* aOutUid16B, uint8_t* pOutLen)
{
  uint8_t mac_id[BD_ADDR_SIZE] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  *pOutLen = BD_ADDR_SIZE;
  FLib_MemCpy(aOutUid16B, mac_id, BD_ADDR_SIZE);
}
