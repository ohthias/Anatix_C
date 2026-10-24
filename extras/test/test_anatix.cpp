// Testes da biblioteca rodando no PC, com um Arduino simulado (Arduino.h
// desta pasta). Rode com:  make -C extras/test
#include "Arduino.h"
#include "Anatix.h"
#include <vector>

uint32_t g_us = 1;
static void tick(uint32_t us) { g_us += us; }

struct FakeSerial : Stream {
  std::string out, in;
  size_t pos = 0;
  int free_ = 1000;
  size_t write(uint8_t c) override { out += (char)c; return 1; }
  int availableForWrite() override { return free_; }
  int available() override { return (int)(in.size() - pos); }
  int read() override { return pos < in.size() ? (uint8_t)in[pos++] : -1; }
} S;

static int falhas = 0;
#define CHECK(cond, msg)                                       \
  do {                                                         \
    if (cond) printf("  ok    %s\n", msg);                     \
    else { printf("  FALHA %s\n", msg); falhas++; }            \
  } while (0)

static std::vector<std::string> linhas(const std::string &s) {
  std::vector<std::string> v;
  size_t a = 0, b;
  while ((b = s.find('\n', a)) != std::string::npos) { v.push_back(s.substr(a, b - a)); a = b + 1; }
  return v;
}

// Valida o formato $ANX,...*CK
static bool frameValido(const std::string &l) {
  if (l.compare(0, 5, "$ANX,") != 0) return false;
  size_t star = l.rfind('*');
  if (star == std::string::npos || l.size() != star + 3) return false;
  uint8_t ck = 0;
  for (size_t i = 1; i < star; i++) ck ^= (uint8_t)l[i];
  return (uint8_t)strtol(l.substr(star + 1).c_str(), nullptr, 16) == ck;
}

static int conta(const std::vector<std::string> &v, const char *tipo) {
  int n = 0;
  for (auto &l : v) if (l.compare(0, 7, std::string("$ANX,") + tipo + ",") == 0) n++;
  return n;
}

static bool contem(const std::string &s, const char *sub) { return s.find(sub) != std::string::npos; }

int main() {
  printf("Anatix — testes\n");
  Anatix.begin(S, "MeuRobo");

  for (int i = 0; i < 10; i++) { ANX_UPDATE(); ANX_VALUE("dist", 12.5f); tick(1000); }
  CHECK(S.out.empty(), "nada e enviado sem handshake");

  S.in += "ANX PING\n";
  S.in += "comando_do_usuario\n";
  for (int i = 0; i < 600; i++) {
    ANX_UPDATE();
    ANX_VALUE("dist_cm", 12.5f + i * 0.1f);
    ANX_VALUE_EVERY("bateria_v", 7.4, 100);
    ANX_VALUE("erro_pid", -3 + (i % 7));
    ANX_STATE("modo", i < 300 ? "seguindo_linha" : "desviando");
    { ANX_SCOPE("pid"); tick(350 + (i % 5) * 20); }
    if (i == 150) ANX_EVENT_DATA("obstaculo", 18);
    if (i == 200) ANX_LOG("msg, com virgula");
    tick(1500);
    if (i % 300 == 0) S.in += "ANX PING\n";
  }
  ANX_VALUE("nan_test", NAN);

  auto v = linhas(S.out);
  bool todosValidos = !v.empty();
  for (auto &l : v) if (!frameValido(l)) { todosValidos = false; printf("        invalido: %s\n", l.c_str()); }

  CHECK(Anatix.connected(), "conecta apos ANX PING");
  CHECK(todosValidos, "todos os frames tem formato e checksum corretos");
  CHECK(conta(v, "H") == 1, "envia um frame hello");
  CHECK(conta(v, "S") == 2, "estado so e enviado quando muda (2 transicoes)");
  int bat = 0;
  for (auto &l : v) if (contem(l, ",bateria_v,")) bat++;
  CHECK(bat >= 9 && bat <= 12, "ANX_VALUE_EVERY respeita o intervalo (~11 envios em 1,1 s)");
  CHECK(conta(v, "T") >= 4, "relatorios de tempo da secao 'pid'");
  CHECK(conta(v, "L") >= 4, "relatorios de loop");
  CHECK(conta(v, "E") == 1, "evento enviado");
  CHECK(contem(S.out, "$ANX,M,") && contem(S.out, "msg, com virgula"), "log com virgula");
  CHECK(contem(S.out, ",nan_test,nan*"), "NaN enviado como 'nan'");

  // Cabo removido: sem PING por mais de ANATIX_TIMEOUT_MS
  for (int i = 0; i < 1200; i++) { ANX_UPDATE(); tick(2000); }
  size_t antes = S.out.size();
  ANX_VALUE("depois", 1);
  CHECK(!Anatix.connected() && S.out.size() == antes, "desliga sozinho apos timeout");

  // Buffer de envio cheio: descarta em vez de travar
  S.in += "ANX PING\n";
  ANX_UPDATE();
  S.free_ = 10;
  antes = S.out.size();
  ANX_VALUE("x", 1);
  CHECK(S.out.size() == antes, "descarta valor com buffer cheio");
  ANX_STATE("modo", "parado");
  CHECK(S.out.size() > antes, "estado e enviado mesmo com buffer cheio");
  S.free_ = 1000;

  // Comandos
  S.in += "ANX STOP\n";
  ANX_UPDATE();
  CHECK(!Anatix.connected(), "ANX STOP desconecta");

  printf(falhas ? "\n%d falha(s)\n" : "\nTodos os testes passaram\n", falhas);
  return falhas ? 1 : 0;
}
