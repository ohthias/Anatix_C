/*
 * Anatix — telemetria em tempo real para o RoboStage
 * ---------------------------------------------------
 * Envia métricas, eventos, estados e tempos de execução do robô pela
 * serial (cabo USB) para o computador, onde o Anatix analisa em tempo real.
 *
 * Princípios:
 *  - Custo quase zero quando o cabo NÃO está conectado: nada é enviado
 *    até o PC fazer o "handshake" (comando "ANX PING").
 *  - Se o PC parar de responder (cabo removido), a telemetria se desliga
 *    sozinha após ANATIX_TIMEOUT_MS.
 *  - Nunca trava o loop: se o buffer da serial estiver cheio, o frame é
 *    descartado e contabilizado (campo "drops").
 *  - Nomes ficam na Flash (F()), economizando RAM em placas AVR (Uno/Nano/Mega).
 *
 * Uso rápido:
 *   #include <Anatix.h>
 *   void setup() { Serial.begin(115200); ANX_BEGIN(Serial, "MeuRobo"); }
 *   void loop() {
 *     ANX_UPDATE();                       // 1x por loop (obrigatório)
 *     ANX_VALUE("distancia_cm", dist);     // métrica
 *     ANX_VALUE_EVERY("bateria_v", v, 500);// métrica limitada a 1 envio/500 ms
 *     ANX_EVENT("obstaculo");              // evento pontual
 *     ANX_STATE("modo", "seguindo_linha"); // estado (só envia quando muda)
 *     { ANX_SCOPE("pid"); calculaPID(); }  // mede o tempo do bloco
 *   }
 *
 * Para remover TODO o código de telemetria da compilação (versão final):
 *   #define ANATIX_DISABLE   // antes do #include <Anatix.h>
 */
#ifndef ANATIX_H
#define ANATIX_H

#include <Arduino.h>

#define ANATIX_PROTOCOL_VERSION 1

// ---- Configuração (pode ser redefinida antes do #include) -----------------
#ifndef ANATIX_MAX_CHANNELS
  #if defined(__AVR__)
    #define ANATIX_MAX_CHANNELS 8      // RAM escassa no Uno/Nano
  #else
    #define ANATIX_MAX_CHANNELS 32
  #endif
#endif

#ifndef ANATIX_TIMEOUT_MS
  #define ANATIX_TIMEOUT_MS 2000       // sem PING por esse tempo => desconecta
#endif

#ifndef ANATIX_REPORT_MS
  #define ANATIX_REPORT_MS 250         // período dos relatórios agregados
#endif

#ifndef ANATIX_RX_BUFFER
  #define ANATIX_RX_BUFFER 32          // tamanho máx. de um comando vindo do PC
#endif

#ifndef ANATIX_FRAME_RESERVE
  #define ANATIX_FRAME_RESERVE 48      // bytes livres exigidos no TX p/ enviar
#endif

typedef void (*AnatixLineHandler)(const char *line);

class AnatixClass {
public:
  // requireHandshake=false envia sempre (útil para ver no Serial Monitor)
  void begin(Stream &port, const char *robotName = "robo",
             bool requireHandshake = true);

  // Chamar uma vez por loop(): lê comandos do PC, mede o loop e
  // envia os relatórios periódicos.
  void update();

  bool connected() const { return _connected; }

  // Métricas numéricas. intervalMs > 0 limita a frequência de envio.
  void value(const __FlashStringHelper *name, float v,         uint16_t intervalMs = 0);
  void value(const __FlashStringHelper *name, double v,        uint16_t intervalMs = 0);
  void value(const __FlashStringHelper *name, int v,           uint16_t intervalMs = 0);
  void value(const __FlashStringHelper *name, unsigned int v,  uint16_t intervalMs = 0);
  void value(const __FlashStringHelper *name, long v,          uint16_t intervalMs = 0);
  void value(const __FlashStringHelper *name, unsigned long v, uint16_t intervalMs = 0);

  // Evento pontual (ex.: "colisao", "linha_perdida").
  void event(const __FlashStringHelper *name, long data = 0);

  // Máquina de estados: só transmite quando o estado muda.
  void state(const __FlashStringHelper *machine, const __FlashStringHelper *st);

  // Cronometragem de trechos de código (agregada: média, máximo, contagem).
  void sectionBegin(const __FlashStringHelper *name);
  void sectionEnd(const __FlashStringHelper *name);

  // Mensagem de texto livre.
  void log(const char *msg);
  void log(const __FlashStringHelper *msg);

  // Linhas recebidas na serial que não são comandos do Anatix
  // (para quem também usa a Serial para outros comandos).
  void setLineHandler(AnatixLineHandler h) { _lineHandler = h; }

  // Altera o período dos relatórios agregados em tempo de execução.
  void setReportPeriod(uint16_t ms) { _reportMs = ms ? ms : 1; }

private:
  enum Kind : uint8_t { K_FREE = 0, K_VALUE, K_STATE, K_SECTION };

  struct Channel {
    uint32_t key;        // hash do nome
    uint32_t a;          // VALUE: último envio (ms) | STATE: hash do estado | SECTION: início (us)
    uint32_t sumUs;      // SECTION: soma das durações no período
    uint32_t maxUs;      // SECTION: maior duração no período
    uint16_t count;      // SECTION: execuções no período
    const __FlashStringHelper *name;
    Kind kind;
  };

  class Out : public Print {        // Print que calcula checksum XOR
  public:
    Stream *port = nullptr;
    uint8_t ck = 0;
    size_t write(uint8_t b) override { ck ^= b; return port->write(b); }
    using Print::write;
  };

  Channel *slot(const __FlashStringHelper *name, Kind kind, bool create);
  static uint32_t hashP(const __FlashStringHelper *s);

  bool frameBegin(char type, bool force = false);  // false => não enviado
  bool valueBegin(const __FlashStringHelper *name, uint16_t intervalMs);
  void frameEnd();
  void sendHello();
  void sendReports(uint32_t nowMs);
  void handleLine(char *line);
  void connect();

  Stream *_port = nullptr;
  Out _out;
  const char *_robotName = "robo";
  bool _requireHandshake = true;
  bool _connected = false;
  bool _txCheck = false;               // a porta informa espaço livre no TX?
  uint32_t _lastPingMs = 0;
  uint32_t _lastReportMs = 0;
  uint16_t _reportMs = ANATIX_REPORT_MS;

  // Estatísticas do loop
  uint32_t _lastLoopUs = 0;
  uint32_t _loopSumUs = 0;
  uint32_t _loopMaxUs = 0;
  uint32_t _loopCount = 0;
  uint16_t _drops = 0;
  bool _channelsFull = false;

  char _rx[ANATIX_RX_BUFFER];
  uint8_t _rxLen = 0;
  AnatixLineHandler _lineHandler = nullptr;

  Channel _ch[ANATIX_MAX_CHANNELS];
};

// Mede o tempo de um bloco {} automaticamente (RAII).
class AnatixScope {
public:
  explicit AnatixScope(const __FlashStringHelper *n);
  ~AnatixScope();
private:
  const __FlashStringHelper *_n;
};

extern AnatixClass Anatix;

// ---- Macros (use estas no código do robô) ---------------------------------
#define ANX_CAT2(a, b) a##b
#define ANX_CAT(a, b) ANX_CAT2(a, b)

#ifndef ANATIX_DISABLE
  #define ANX_BEGIN(port, name)          Anatix.begin(port, name)
  #define ANX_UPDATE()                   Anatix.update()
  #define ANX_VALUE(name, v)             Anatix.value(F(name), (v))
  #define ANX_VALUE_EVERY(name, v, ms)   Anatix.value(F(name), (v), (ms))
  #define ANX_EVENT(name)                Anatix.event(F(name))
  #define ANX_EVENT_DATA(name, data)     Anatix.event(F(name), (long)(data))
  #define ANX_STATE(machine, st)         Anatix.state(F(machine), F(st))
  #define ANX_SECTION_BEGIN(name)        Anatix.sectionBegin(F(name))
  #define ANX_SECTION_END(name)          Anatix.sectionEnd(F(name))
  #define ANX_SCOPE(name)                AnatixScope ANX_CAT(_anxScope, __LINE__)(F(name))
  #define ANX_LOG(msg)                   Anatix.log(F(msg))
#else
  #define ANX_BEGIN(port, name)          ((void)0)
  #define ANX_UPDATE()                   ((void)0)
  #define ANX_VALUE(name, v)             ((void)0)
  #define ANX_VALUE_EVERY(name, v, ms)   ((void)0)
  #define ANX_EVENT(name)                ((void)0)
  #define ANX_EVENT_DATA(name, data)     ((void)0)
  #define ANX_STATE(machine, st)         ((void)0)
  #define ANX_SECTION_BEGIN(name)        ((void)0)
  #define ANX_SECTION_END(name)          ((void)0)
  #define ANX_SCOPE(name)                ((void)0)
  #define ANX_LOG(msg)                   ((void)0)
#endif

#endif // ANATIX_H
