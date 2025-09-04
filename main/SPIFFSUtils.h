#ifndef SPIFFSUTILS_H
#define SPIFFSUTILS_H

#include <Arduino.h>

void listSPIFFS(void);
bool getFile(String url, String filename);

#endif
