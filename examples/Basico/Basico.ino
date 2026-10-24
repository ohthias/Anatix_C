/*
 * Exemplo Anatix — o mínimo para começar
 *
 * Lê um potenciômetro em A0 e pisca o LED da placa.
 * Conecte o cabo USB e abra o Anatix no RoboStage para ver os dados ao vivo.
 */
#include <Anatix.h>

void setup() {
  Serial.begin(115200);
  ANX_BEGIN(Serial, "Teste");
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  ANX_UPDATE();

  int leitura = analogRead(A0);
  ANX_VALUE("potenciometro", leitura);

  bool ligado = (millis() / 500) % 2;
  digitalWrite(LED_BUILTIN, ligado);
  if (ligado) ANX_STATE("led", "ligado");
  else        ANX_STATE("led", "desligado");

  if (leitura > 900) ANX_EVENT("leitura_alta");
}
