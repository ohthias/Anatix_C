// Garante que, com ANATIX_DISABLE, todas as macros compilam e não geram código.
#define ANATIX_DISABLE
#include "Arduino.h"
#include "Anatix.h"

uint32_t g_us = 0;

int main() {
  ANX_UPDATE();
  ANX_VALUE("a", 1);
  ANX_VALUE_EVERY("b", 2.0, 5);
  ANX_EVENT("c");
  ANX_EVENT_DATA("d", 3.5f);
  ANX_STATE("m", "s");
  { ANX_SCOPE("x"); }
  ANX_SECTION_BEGIN("y");
  ANX_SECTION_END("y");
  ANX_LOG("z");
  return 0;
}
