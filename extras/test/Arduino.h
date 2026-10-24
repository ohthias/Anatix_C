// Arduino.h simulado para rodar os testes no PC (não é usado na placa).
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <cstdio>
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define pgm_read_byte(p) (*(const uint8_t *)(p))
extern uint32_t g_us;
inline uint32_t micros() { return g_us; }
inline uint32_t millis() { return g_us / 1000; }
class Print {
public:
  virtual size_t write(uint8_t) = 0;
  virtual int availableForWrite() { return 0; }
  size_t print(const char *s) { size_t n=0; while (*s) n += write(*s++); return n; }
  size_t print(const __FlashStringHelper *s) { return print((const char*)s); }
  size_t print(long v) { char b[24]; snprintf(b,24,"%ld",v); return print(b); }
  size_t print(unsigned long v) { char b[24]; snprintf(b,24,"%lu",v); return print(b); }
  size_t print(int v) { return print((long)v); }
  size_t print(unsigned int v) { return print((unsigned long)v); }
  size_t print(double v, int d=2) { char b[40]; snprintf(b,40,"%.*f",d,v); return print(b); }
};
class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
};
