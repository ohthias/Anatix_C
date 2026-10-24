# Anatix para Arduino

Biblioteca de telemetria em tempo real para robôs, feita para o **Anatix do RoboStage**.
Com o cabo USB conectado, o robô envia métricas, eventos, estados e tempos de
execução para o computador, que analisa o desempenho ao vivo.

- **Sem cabo, sem custo:** nada é transmitido até o computador pedir.
- **Nunca trava o robô:** se a serial estiver cheia, o dado é descartado e contado.
- **Leve:** sem alocação dinâmica; nomes ficam na Flash (não gastam RAM).
- **Some na competição:** `#define ANATIX_DISABLE` remove todo o código de telemetria.

Funciona em AVR (Uno, Nano, Mega, Leonardo), ESP32, ESP8266, RP2040, SAMD e outras
placas com `Serial`.

## Instalação

**Arduino IDE:** baixe o repositório como .zip e use
*Sketch → Incluir Biblioteca → Adicionar biblioteca .ZIP*.

**Manual:** copie a pasta para `Documentos/Arduino/libraries/Anatix`.

**PlatformIO:** em `platformio.ini`, adicione o repositório a `lib_deps`.

## Uso

```cpp
#include <Anatix.h>

void setup() {
  Serial.begin(115200);
  ANX_BEGIN(Serial, "MeuRobo");
}

void loop() {
  ANX_UPDATE();                              // obrigatório, 1x por loop

  ANX_VALUE("distancia_cm", dist);           // métrica (gráfico)
  ANX_VALUE_EVERY("bateria_v", bat, 1000);   // métrica, no máx. 1 envio a cada 1000 ms
  ANX_EVENT("linha_perdida");                // evento pontual
  ANX_EVENT_DATA("obstaculo", dist);         // evento com um número
  ANX_STATE("modo", "resgate");              // estado; só envia quando muda
  { ANX_SCOPE("pid"); calculaPID(); }        // mede o tempo do bloco
  ANX_SECTION_BEGIN("giro"); girar90(); ANX_SECTION_END("giro");  // idem, manual
  ANX_LOG("calibracao concluida");           // texto livre
}
```

Veja os exemplos em [`examples/`](examples): `Basico` e `SeguidorDeLinha` (PID + ultrassom).

### Referência

| Macro | O que envia |
|---|---|
| `ANX_BEGIN(porta, nome)` | inicia; `porta` é qualquer `Stream` (`Serial`, `Serial1`…) |
| `ANX_UPDATE()` | lê comandos do computador, mede o loop e envia relatórios |
| `ANX_VALUE(nome, v)` | valor numérico (int, long, unsigned, float, double) |
| `ANX_VALUE_EVERY(nome, v, ms)` | idem, com limite de frequência |
| `ANX_EVENT(nome)` / `ANX_EVENT_DATA(nome, n)` | evento, com número opcional |
| `ANX_STATE(maquina, estado)` | estado atual de uma máquina de estados |
| `ANX_SCOPE(nome)` | cronometra o bloco `{ }` onde está |
| `ANX_SECTION_BEGIN(nome)` / `ANX_SECTION_END(nome)` | cronometra um trecho |
| `ANX_LOG(texto)` | mensagem de texto |

Automaticamente, a cada 250 ms: **loops por segundo**, tempo médio e pior tempo do
loop, **RAM livre** e quantos dados foram **descartados**.

**Regras:** nomes e estados devem ser textos fixos entre aspas e sem vírgulas.
Uma seção não pode ser aninhada nela mesma.

### Métodos da classe

As macros chamam o objeto global `Anatix` (classe `AnatixClass`). Ele também oferece:

```cpp
Anatix.connected();                 // true enquanto o computador estiver ouvindo
Anatix.setReportPeriod(500);        // período dos relatórios automáticos (ms)
Anatix.setLineHandler(minhaFuncao); // recebe linhas da Serial que não são do Anatix
```

Use `setLineHandler` se o seu código também lê comandos pela `Serial`: o Anatix lê
as linhas e repassa as que não são dele.

```cpp
void meusComandos(const char *linha) {
  if (strcmp(linha, "parar") == 0) pararMotores();
}
// no setup(): Anatix.setLineHandler(meusComandos);
```

### Configuração

Defina antes do `#include <Anatix.h>`:

| Define | Padrão | Descrição |
|---|---|---|
| `ANATIX_DISABLE` | — | remove todo o código de telemetria |
| `ANATIX_MAX_CHANNELS` | 8 (AVR) / 32 | métricas com `_EVERY` + estados + seções |
| `ANATIX_TIMEOUT_MS` | 2000 | tempo sem contato até parar de transmitir |
| `ANATIX_REPORT_MS` | 250 | período dos relatórios automáticos |
| `ANATIX_RX_BUFFER` | 32 | tamanho máximo de um comando recebido |

### Dicas de desempenho

A 115200 baud cabem cerca de 400 mensagens por segundo. Em loops rápidos (Uno/Nano),
use `ANX_VALUE_EVERY` nas métricas que não precisam de todas as amostras e fique de
olho no contador de descartes. Para mais vazão, use 250000 ou 500000 baud dos dois lados.

## Protocolo

Texto simples, uma mensagem por linha, com checksum. A especificação completa, para
quem for implementar o lado do computador, está em [`docs/PROTOCOLO.md`](docs/PROTOCOLO.md).

```
$ANX,V,1520,distancia_cm,23.400*23
```

Para inspecionar sem o site: abra o Monitor Serial a 115200 baud, envie `ANX PING`
e veja as mensagens chegando (elas param após 2 s sem novo PING).

## Estrutura

```
├── library.properties     metadados da biblioteca
├── keywords.txt           destaque de sintaxe na Arduino IDE
├── src/                   Anatix.h, Anatix.cpp
├── examples/              Basico, SeguidorDeLinha
├── docs/PROTOCOLO.md      especificação do protocolo
└── extras/test/           testes no PC (ignorados pela Arduino IDE)
```
