/*
 * Anatix — implementação
 *
 * Formato de cada frame (uma linha de texto ASCII):
 *
 *   $ANX,<tipo>,<millis>,<campos...>*<CK>\n
 *
 *   CK = XOR (em hex, 2 dígitos) de todos os bytes entre '$' e '*'.
 *
 *   H  hello     $ANX,H,ms,<versao>,<robo>,<placa>,<periodo_ms>
 *   V  valor     $ANX,V,ms,<nome>,<valor>
 *   E  evento    $ANX,E,ms,<nome>,<dado>
 *   S  estado    $ANX,S,ms,<maquina>,<estado>
 *   T  tempo     $ANX,T,ms,<secao>,<execucoes>,<media_us>,<max_us>
 *   L  loop      $ANX,L,ms,<loops_por_s>,<media_us>,<max_us>,<ram_livre>,<descartes>
 *   M  mensagem  $ANX,M,ms,<texto>
 *
 * Comandos aceitos do computador (uma linha cada):
 *   ANX PING        mantém a conexão viva (conecta na primeira vez)
 *   ANX HELLO       pede o frame H novamente
 *   ANX STOP        para de transmitir
 *   ANX RATE <ms>   muda o período dos relatórios agregados (L e T)
 *   ANX RESET       zera as estatísticas agregadas
 */
#include "Anatix.h"

#if defined(ESP32) || defined(ESP8266)
  #include <Esp.h>
#endif

AnatixClass Anatix;

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

static const char *boardName() {
#if defined(__AVR_ATmega328P__)
  return "atmega328p";
#elif defined(__AVR_ATmega2560__)
  return "atmega2560";
#elif defined(__AVR_ATmega32U4__)
  return "atmega32u4";
#elif defined(__AVR__)
  return "avr";
#elif defined(ESP32)
  return "esp32";
#elif defined(ESP8266)
  return "esp8266";
#elif defined(ARDUINO_ARCH_RP2040)
  return "rp2040";
#elif defined(ARDUINO_ARCH_SAMD)
  return "samd";
#elif defined(ARDUINO_ARCH_RENESAS)
  return "renesas";
#elif defined(ARDUINO_ARCH_STM32)
  return "stm32";
#else
  return "desconhecida";
#endif
}

static long freeRam() {
#if defined(__AVR__)
  extern int __heap_start, *__brkval;
  int topo;
  return (long)((int)&topo - (__brkval == 0 ? (int)&__heap_start : (int)__brkval));
#elif defined(ESP32) || defined(ESP8266)
  return (long)ESP.getFreeHeap();
#else
  return -1;
#endif
}

// FNV-1a sobre uma string na Flash (F()).
uint32_t AnatixClass::hashP(const __FlashStringHelper *s) {
  const char *p = reinterpret_cast<const char *>(s);
  uint32_t h = 2166136261UL;
  for (;;) {
    uint8_t c = pgm_read_byte(p++);
    if (!c) break;
    h ^= c;
    h *= 16777619UL;
  }
  return h ? h : 1;  // 0 é reservado para "livre"
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------

void AnatixClass::begin(Stream &port, const char *robotName, bool requireHandshake) {
  _port = &port;
  _out.port = &port;
  _robotName = robotName ? robotName : "robo";
  _requireHandshake = requireHandshake;
  // Algumas portas não informam o espaço livre no TX (retornam 0).
  _txCheck = port.availableForWrite() > 0;
  _rxLen = 0;
  for (uint8_t i = 0; i < ANATIX_MAX_CHANNELS; i++) _ch[i].kind = K_FREE;
  _connected = false;
  if (!_requireHandshake) connect();
}

void AnatixClass::connect() {
  _connected = true;
  _lastPingMs = millis();
  _lastReportMs = millis();
  _loopSumUs = _loopMaxUs = _loopCount = 0;
  _drops = 0;
  // Força o reenvio dos estados atuais e zera as seções.
  for (uint8_t i = 0; i < ANATIX_MAX_CHANNELS; i++) {
    Channel &c = _ch[i];
    if (c.kind == K_STATE) c.a = 0;
    if (c.kind == K_SECTION) { c.sumUs = c.maxUs = 0; c.count = 0; }
    if (c.kind == K_VALUE) c.a = 0;
  }
  sendHello();
}

void AnatixClass::update() {
  if (!_port) return;

  // 1) Comandos vindos do computador
  while (_port->available() > 0) {
    int b = _port->read();
    if (b < 0) break;
    char c = (char)b;
    if (c == '\r') continue;
    if (c == '\n') {
      _rx[_rxLen] = '\0';
      if (_rxLen > 0) handleLine(_rx);
      _rxLen = 0;
    } else if (_rxLen < ANATIX_RX_BUFFER - 1) {
      _rx[_rxLen++] = c;
    } else {
      _rxLen = 0;  // linha longa demais: descarta
    }
  }

  // 2) Tempo de loop
  uint32_t nowUs = micros();
  if (_lastLoopUs != 0) {
    uint32_t d = nowUs - _lastLoopUs;
    _loopSumUs += d;
    if (d > _loopMaxUs) _loopMaxUs = d;
    _loopCount++;
  }
  _lastLoopUs = nowUs | 1;

  if (!_connected) return;
  uint32_t nowMs = millis();

  // 3) Cabo desconectado / aplicativo fechado
  if (_requireHandshake && (nowMs - _lastPingMs) > ANATIX_TIMEOUT_MS) {
    _connected = false;
    return;
  }

  // 4) Relatórios agregados
  if ((nowMs - _lastReportMs) >= _reportMs) sendReports(nowMs);
}

void AnatixClass::handleLine(char *line) {
  if (strncmp(line, "ANX ", 4) != 0) {
    if (_lineHandler) _lineHandler(line);
    return;
  }
  const char *cmd = line + 4;
  if (strcmp(cmd, "PING") == 0) {
    _lastPingMs = millis();
    if (!_connected) connect();
  } else if (strcmp(cmd, "HELLO") == 0) {
    if (_connected) sendHello();
  } else if (strcmp(cmd, "STOP") == 0) {
    _connected = false;
  } else if (strncmp(cmd, "RATE ", 5) == 0) {
    long ms = atol(cmd + 5);
    if (ms >= 20 && ms <= 10000) _reportMs = (uint16_t)ms;
  } else if (strcmp(cmd, "RESET") == 0) {
    _loopSumUs = _loopMaxUs = _loopCount = 0;
    _drops = 0;
    for (uint8_t i = 0; i < ANATIX_MAX_CHANNELS; i++)
      if (_ch[i].kind == K_SECTION) { _ch[i].sumUs = _ch[i].maxUs = 0; _ch[i].count = 0; }
  }
}

// ---------------------------------------------------------------------------
// Canais (tabela fixa, sem alocação dinâmica)
// ---------------------------------------------------------------------------

AnatixClass::Channel *AnatixClass::slot(const __FlashStringHelper *name, Kind kind, bool create) {
  uint32_t key = hashP(name) ^ ((uint32_t)kind << 29);
  if (!key) key = 1;
  Channel *livre = nullptr;
  for (uint8_t i = 0; i < ANATIX_MAX_CHANNELS; i++) {
    Channel &c = _ch[i];
    if (c.kind == kind && c.key == key) return &c;
    if (c.kind == K_FREE && !livre) livre = &c;
  }
  if (!create) return nullptr;
  if (!livre) {
    if (!_channelsFull && _connected) {
      _channelsFull = true;
      log(F("anatix: canais esgotados, aumente ANATIX_MAX_CHANNELS"));
    }
    return nullptr;
  }
  livre->key = key;
  livre->kind = kind;
  livre->name = name;
  livre->a = livre->sumUs = livre->maxUs = 0;
  livre->count = 0;
  return livre;
}

// ---------------------------------------------------------------------------
// Montagem de frames
// ---------------------------------------------------------------------------

// force=false: se o buffer de envio estiver cheio, descarta o frame (conta em
// "descartes") para nunca travar o loop do robô.
// force=true: frames importantes (hello, estado, log) esperam espaço.
bool AnatixClass::frameBegin(char type, bool force) {
  if (!_connected || !_port) return false;
  if (!force && _txCheck && _port->availableForWrite() < ANATIX_FRAME_RESERVE) {
    if (_drops < 0xFFFF) _drops++;
    return false;
  }
  _port->write('$');
  _out.ck = 0;
  _out.print(F("ANX,"));
  _out.write((uint8_t)type);
  _out.write(',');
  _out.print(millis());
  return true;
}

void AnatixClass::frameEnd() {
  static const char hex[] = "0123456789ABCDEF";
  uint8_t ck = _out.ck;
  _port->write('*');
  _port->write(hex[ck >> 4]);
  _port->write(hex[ck & 0x0F]);
  _port->write('\n');
}

void AnatixClass::sendHello() {
  if (!frameBegin('H', true)) return;
  _out.write(',');
  _out.print(ANATIX_PROTOCOL_VERSION);
  _out.write(',');
  _out.print(_robotName);
  _out.write(',');
  _out.print(boardName());
  _out.write(',');
  _out.print(_reportMs);
  frameEnd();
}

void AnatixClass::sendReports(uint32_t nowMs) {
  uint32_t elapsed = nowMs - _lastReportMs;
  _lastReportMs = nowMs;
  if (elapsed == 0) elapsed = 1;

  if (frameBegin('L')) {
    uint32_t hz = (uint32_t)(((uint64_t)_loopCount * 1000UL) / elapsed);
    uint32_t avg = _loopCount ? _loopSumUs / _loopCount : 0;
    _out.write(','); _out.print(hz);
    _out.write(','); _out.print(avg);
    _out.write(','); _out.print(_loopMaxUs);
    _out.write(','); _out.print(freeRam());
    _out.write(','); _out.print(_drops);
    frameEnd();
    _drops = 0;
  }
  _loopSumUs = _loopMaxUs = _loopCount = 0;

  for (uint8_t i = 0; i < ANATIX_MAX_CHANNELS; i++) {
    Channel &c = _ch[i];
    if (c.kind != K_SECTION || c.count == 0) continue;
    if (frameBegin('T')) {
      _out.write(','); _out.print(c.name);
      _out.write(','); _out.print(c.count);
      _out.write(','); _out.print(c.sumUs / c.count);
      _out.write(','); _out.print(c.maxUs);
      frameEnd();
    }
    c.sumUs = c.maxUs = 0;
    c.count = 0;
  }
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

// Decide se um valor deve ser enviado (conexão, limite de frequência,
// espaço no buffer) e já escreve o cabeçalho + nome. Se retornar true,
// o chamador imprime o valor e chama frameEnd().
bool AnatixClass::valueBegin(const __FlashStringHelper *name, uint16_t intervalMs) {
  if (!_connected) return false;
  if (intervalMs) {
    Channel *c = slot(name, K_VALUE, true);
    if (c) {
      uint32_t now = millis();
      if (c->a && (now - c->a) < intervalMs) return false;
      c->a = now | 1;
    }
  }
  if (!frameBegin('V')) return false;
  _out.write(',');
  _out.print(name);
  _out.write(',');
  return true;
}

void AnatixClass::value(const __FlashStringHelper *name, float v, uint16_t intervalMs) {
  value(name, (double)v, intervalMs);
}
void AnatixClass::value(const __FlashStringHelper *name, double v, uint16_t intervalMs) {
  if (!valueBegin(name, intervalMs)) return;
  if (isnan(v)) _out.print(F("nan"));
  else if (isinf(v)) _out.print(v > 0 ? "inf" : "-inf");
  else _out.print(v, 3);
  frameEnd();
}
void AnatixClass::value(const __FlashStringHelper *name, int v, uint16_t intervalMs) {
  value(name, (long)v, intervalMs);
}
void AnatixClass::value(const __FlashStringHelper *name, unsigned int v, uint16_t intervalMs) {
  value(name, (unsigned long)v, intervalMs);
}
void AnatixClass::value(const __FlashStringHelper *name, long v, uint16_t intervalMs) {
  if (!valueBegin(name, intervalMs)) return;
  _out.print(v);
  frameEnd();
}
void AnatixClass::value(const __FlashStringHelper *name, unsigned long v, uint16_t intervalMs) {
  if (!valueBegin(name, intervalMs)) return;
  _out.print(v);
  frameEnd();
}

void AnatixClass::event(const __FlashStringHelper *name, long data) {
  if (!frameBegin('E')) return;
  _out.write(',');
  _out.print(name);
  _out.write(',');
  _out.print(data);
  frameEnd();
}

void AnatixClass::state(const __FlashStringHelper *machine, const __FlashStringHelper *st) {
  if (!_connected) return;
  Channel *c = slot(machine, K_STATE, true);
  uint32_t h = hashP(st);
  if (c && c->a == h) return;          // estado não mudou
  if (!frameBegin('S', true)) return;   // mudança de estado não pode se perder
  if (c) c->a = h;
  _out.write(',');
  _out.print(machine);
  _out.write(',');
  _out.print(st);
  frameEnd();
}

void AnatixClass::sectionBegin(const __FlashStringHelper *name) {
  if (!_connected) return;
  Channel *c = slot(name, K_SECTION, true);
  if (c) c->a = micros() | 1;
}

void AnatixClass::sectionEnd(const __FlashStringHelper *name) {
  if (!_connected) return;
  uint32_t now = micros();
  Channel *c = slot(name, K_SECTION, false);
  if (!c || !c->a) return;
  uint32_t d = now - c->a;
  c->a = 0;
  c->sumUs += d;
  if (d > c->maxUs) c->maxUs = d;
  if (c->count < 0xFFFF) c->count++;
}

void AnatixClass::log(const char *msg) {
  if (!frameBegin('M', true)) return;
  _out.write(',');
  for (const char *p = msg; *p; p++) {
    char c = *p;
    _out.write((uint8_t)((c == '\n' || c == '\r') ? ' ' : c));
  }
  frameEnd();
}

void AnatixClass::log(const __FlashStringHelper *msg) {
  if (!frameBegin('M', true)) return;
  _out.write(',');
  const char *p = reinterpret_cast<const char *>(msg);
  for (;;) {
    char c = pgm_read_byte(p++);
    if (!c) break;
    _out.write((uint8_t)((c == '\n' || c == '\r') ? ' ' : c));
  }
  frameEnd();
}

// ---------------------------------------------------------------------------
// AnatixScope
// ---------------------------------------------------------------------------

AnatixScope::AnatixScope(const __FlashStringHelper *n) : _n(n) { Anatix.sectionBegin(n); }
AnatixScope::~AnatixScope() { Anatix.sectionEnd(_n); }
