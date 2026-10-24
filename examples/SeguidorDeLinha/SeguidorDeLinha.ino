/*
 * Exemplo Anatix — seguidor de linha (OBR Resgate) com PID
 *
 * Conecte o cabo USB, abra o Anatix no RoboStage (Chrome/Edge) e clique em
 * "Conectar robô". Sem o cabo, o robô roda normalmente e a telemetria
 * fica desligada.
 *
 * Ajuste os pinos para o seu robô.
 */
// #define ANATIX_DISABLE        // descomente para a versão de competição
#include <Anatix.h>

const uint8_t SENSOR_ESQ = A0;
const uint8_t SENSOR_DIR = A1;
const uint8_t ULTRASSOM_TRIG = 7;
const uint8_t ULTRASSOM_ECHO = 8;
const uint8_t MOTOR_ESQ_PWM = 5;
const uint8_t MOTOR_DIR_PWM = 6;
const uint8_t BATERIA = A2;      // divisor de tensão 1:2

float kp = 0.8, ki = 0.0, kd = 4.0;
float erroAnterior = 0, integral = 0;
const int VELOCIDADE_BASE = 150;

float lerDistanciaCm() {
  digitalWrite(ULTRASSOM_TRIG, LOW);  delayMicroseconds(2);
  digitalWrite(ULTRASSOM_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(ULTRASSOM_TRIG, LOW);
  unsigned long duracao = pulseIn(ULTRASSOM_ECHO, HIGH, 20000UL);
  return duracao ? duracao / 58.0f : 999.0f;
}

void setup() {
  Serial.begin(115200);
  ANX_BEGIN(Serial, "Seguidor-OBR");

  pinMode(ULTRASSOM_TRIG, OUTPUT);
  pinMode(ULTRASSOM_ECHO, INPUT);
  pinMode(MOTOR_ESQ_PWM, OUTPUT);
  pinMode(MOTOR_DIR_PWM, OUTPUT);
}

void loop() {
  ANX_UPDATE();   // sempre no início do loop

  // --- Sensores ----------------------------------------------------------
  int esq, dir;
  {
    ANX_SCOPE("leitura_sensores");          // quanto tempo a leitura leva
    esq = analogRead(SENSOR_ESQ);
    dir = analogRead(SENSOR_DIR);
  }
  ANX_VALUE("sensor_esq", esq);
  ANX_VALUE("sensor_dir", dir);

  float distancia;
  {
    ANX_SCOPE("ultrassom");                 // pulseIn costuma ser o vilão
    distancia = lerDistanciaCm();
  }
  ANX_VALUE("distancia_cm", distancia);

  float bateria = analogRead(BATERIA) * (5.0f / 1023.0f) * 2.0f;
  ANX_VALUE_EVERY("bateria_v", bateria, 1000);   // 1 vez por segundo basta

  // --- Decisão -----------------------------------------------------------
  if (distancia < 10) {
    ANX_STATE("modo", "desviando");
    static bool avisou = false;
    if (!avisou) { ANX_EVENT_DATA("obstaculo", distancia); avisou = true; }
    analogWrite(MOTOR_ESQ_PWM, 0);
    analogWrite(MOTOR_DIR_PWM, 0);
    return;
  }
  ANX_STATE("modo", "seguindo_linha");

  // --- PID ---------------------------------------------------------------
  int motorEsq, motorDir;
  {
    ANX_SCOPE("pid");
    float erro = (float)(esq - dir);
    integral += erro;
    float derivada = erro - erroAnterior;
    erroAnterior = erro;
    float correcao = kp * erro + ki * integral + kd * derivada;

    motorEsq = constrain(VELOCIDADE_BASE + (int)correcao, 0, 255);
    motorDir = constrain(VELOCIDADE_BASE - (int)correcao, 0, 255);

    ANX_VALUE("pid_erro", erro);
    ANX_VALUE("pid_correcao", correcao);
  }

  analogWrite(MOTOR_ESQ_PWM, motorEsq);
  analogWrite(MOTOR_DIR_PWM, motorDir);
  ANX_VALUE("motor_esq", motorEsq);
  ANX_VALUE("motor_dir", motorDir);

  if (abs(esq - dir) > 700) ANX_EVENT("linha_perdida");
}
