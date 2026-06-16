#include "kpm/kpm.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Get integer converted format of a device code
 * 
 * @param deviceCode 2 or 3 character code
 * @return int 
 */
int getDeviceCodeFromStringHD(char* deviceCode)
{
  if (strlen(deviceCode) == 2)
  {
    return strtol(deviceCode, NULL, 16);
  }

  if (strlen(deviceCode) != 3)
  {
    return 0;
  }

  char* converted = malloc(4);
  converted[3] = '\0';

  char* currentConvertedChar = converted;
  for (int i=0; i < 3; i++)
  {
    // We do a little hackerdude logic
    char currentChar = *deviceCode;
    char capitalised = currentChar & 0xdf;

    if (capitalised == 'I' || capitalised == 'O' || capitalised > 'X') {
      return 0;
    }

    if (capitalised > 'I')
    {
      currentChar -= 1;
    }
    if (capitalised > 'O')
    {
      currentChar -= 1;
    }

    *(currentConvertedChar++) = currentChar;
    deviceCode++;
  }

  return strtol(converted, NULL, 32);
}