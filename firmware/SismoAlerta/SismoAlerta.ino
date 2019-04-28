/*
  Copyright 2015 Manuel Rodrigo Rábade García <manuel@rabade.net>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Wire.h"
#include <TimerOne.h>
#include <Si4707.h>
#include "SismoAlerta.h"
#if (YUN_LOGGER || YUN_TWITTER)
#include <Bridge.h>
#include <Console.h>
#if YUN_LOGGER
#include <Process.h>
#endif
#if YUN_TWITTER
#include <HttpClient.h>
#include "yun_twitter.h"
#endif
#endif

/* variables y objetos globales
   ============================ */

// weather band radio
Si4707 wbr(SI4707_RESET, SI4707_ENABLE);
const unsigned long wbr_channels[] = { WBR_CHANNELS };

// maquina de estados
enum fsm_states { SCAN, LISTEN } state = SCAN;

// escaneo
boolean scan_error_flag = 0;
byte tune_channel;

// monitoreo
boolean asq_prev_status;
byte same_prev_state, same_headers_count;
unsigned long tune_timer, same_timer, same_test_timer;

// servicio al usuario
volatile unsigned long user_test_timer = 0;
volatile unsigned long same_alert_timer = 0;
volatile unsigned int user_button_integrator = 0;
volatile boolean user_button_prev_state = HIGH;
volatile unsigned long last_user_button_push = 0;
volatile unsigned int ext_power_analog_avg;
volatile unsigned long last_update = 0;
volatile boolean update_state = LOW;

// monitoreo calidad de señal
#if SNR_RSSI_LOG
float snr_avg, rssi_avg;
unsigned long snr_rssi_sample_timer;
int snr_rssi_samples_count;
#endif

// modo arduino yún con bitácora
#if YUN_LOGGER
Process proc;
#endif

// modo arduino yún con retransmisor twitter
#if YUN_TWITTER
HttpClient http;
#endif

/* configuración
   ============= */

void setup() {
  // consola telnet o puerto serial
#if (YUN_LOGGER || YUN_TWITTER)
  Bridge.begin();
  LOG.begin();
#if YUN_LOGGER
  // proceso logger (nc localhost 6571 | ...)
  proc.runShellCommandAsynchronously("/root/sismo_alerta_logger");
  while (!LOG);
#endif
#else
  LOG.begin(9600);
  LOG.println();
#endif

  // entrada/salida
  LOG.println(F("SETUP"));
  pinMode(BUZZER, OUTPUT);
  pinMode(POWER_LED_RED, OUTPUT);
  pinMode(POWER_LED_GREEN, OUTPUT);
  pinMode(SIGNAL_LED_RED, OUTPUT);
  pinMode(SIGNAL_LED_GREEN, OUTPUT);
  pinMode(USER_BUTTON, INPUT_PULLUP);

  // autoprueba
  LOG.println(F("SELFTEST"));
  digitalWrite(POWER_LED_GREEN, HIGH);
  digitalWrite(SIGNAL_LED_GREEN, HIGH);
  digitalWrite(POWER_LED_RED, LOW);
  digitalWrite(SIGNAL_LED_RED, LOW);
  digitalWrite(BUZZER, HIGH);
  delay(SELFTEST_DELAY);
  digitalWrite(POWER_LED_GREEN, LOW);
  digitalWrite(SIGNAL_LED_GREEN, LOW);
  digitalWrite(POWER_LED_RED, HIGH);
  digitalWrite(SIGNAL_LED_RED, HIGH);
  digitalWrite(BUZZER, LOW);
  delay(SELFTEST_DELAY);
  digitalWrite(POWER_LED_RED, LOW);
  digitalWrite(SIGNAL_LED_RED, LOW);

  // si4707
  LOG.println(F("BEGIN"));
  if (wbr.begin()) {
    // interrupción de servicio al usuario
    ext_power_analog_avg = analogRead(EXT_POWER);
    Timer1.initialize(SERVICE_USER_MICROSEC);
    Timer1.attachInterrupt(service_user);
    // configuración si4707
    wbr.setMuteVolume(1);
    wbr.setSNR(TUNE_MIN_SNR);
    wbr.setRSSI(TUNE_MIN_RSSI);
  } else {
    // error si4707
    digitalWrite(POWER_LED_RED, HIGH);
    digitalWrite(POWER_LED_GREEN, LOW);
    digitalWrite(SIGNAL_LED_RED, LOW);
    digitalWrite(SIGNAL_LED_GREEN, LOW);
    LOG.println(F("ERROR"));
    while (1);
  }
}

/* maquina de estados
   ================== */

void loop() {
  switch (state) {
  case SCAN:
    scan();
    break;
  case LISTEN:
    listen();
    break;
  }
}

/* escaneo
   ======= */

void scan() {
  LOG.println(F("SCAN_START"));

  // promedios de rssi y snr
  float scan_rssi_avg[WBR_CHANNELS_SIZE] = { 0 };
  float scan_snr_avg[WBR_CHANNELS_SIZE] = { 0 };

  // escaneo
  for (byte t = 0; t < SCAN_TIMES; t++) {
    for (byte c = 0; c < WBR_CHANNELS_SIZE; c++) {
      wbr.setWBFrequency(wbr_channels[c]);
      for (byte s = 0; s < SCAN_SAMPLES; s++) {
        delay(SCAN_SAMPLE_DELAY);
        // muestreo
        unsigned int rssi = wbr.getRSSI();
        unsigned int snr = wbr.getSNR();
        // promedio móvil acumulativo
        scan_rssi_avg[c] += (rssi - scan_rssi_avg[c]) / (3 * t + s + 1);
        scan_snr_avg[c] += (snr - scan_snr_avg[c]) / (3 * t + s + 1);
        // reporte muestra
#if SCAN_VERBOSE
        LOG.print(F("SCAN_SAMPLE,"));
        LOG.print(wbr_channels[c], DEC);
        LOG.print(F(","));
        LOG.print(3 * t + s, DEC);
        LOG.print(F(","));
        LOG.print(rssi, DEC);
        LOG.print(F(","));
        LOG.println(snr, DEC);
#endif
      }
    }
    delay(SCAN_DELAY);
  }

  // mejor canal
  byte best_channel = 0;
  float best_rssi = 0;
  float best_snr = 0;

  // reporte promedios y búsqueda del mejor canal
  for (byte c = 0; c < WBR_CHANNELS_SIZE; c++) {
    LOG.print(F("SCAN_AVG,"));
    LOG.print(wbr_channels[c], DEC);
    LOG.print(F(","));
    LOG.print(SCAN_TIMES * SCAN_SAMPLES, DEC);
    LOG.print(F(","));
    LOG.print(scan_rssi_avg[c], 2);
    LOG.print(F(","));
    LOG.println(scan_snr_avg[c], 2);
    if (scan_rssi_avg[c] >= best_rssi && scan_snr_avg[c] >= best_snr) {
      best_channel = c;
      best_rssi = scan_rssi_avg[c];
      best_snr = scan_snr_avg[c];
    }
  }

  // ¿se encontró un canal valido?
  if (best_rssi >= (float) TUNE_MIN_RSSI && best_snr >= (float) TUNE_MIN_SNR) {
    // sintonizar canal y pasar a estado monitoreo
    state = LISTEN;
    scan_error_flag = 0;
    tune_channel = best_channel;
    wbr.setWBFrequency(wbr_channels[tune_channel]);
    LOG.print(F("SCAN_OK,"));
    LOG.println(wbr_channels[best_channel], DEC);
    // reinicio monitoreo
    asq_prev_status = 0;
    same_prev_state = 0;
    same_headers_count = 0;
    tune_timer = millis();
    same_timer = 0;
    same_test_timer = 0;
    // reinicio monitoreo calidad de señal
#if SNR_RSSI_LOG
    snr_rssi_sample_timer = millis();
    rssi_avg = 0;
    snr_avg = 0;
    snr_rssi_samples_count = 0;
#endif
  } else {
    // repetir escaneo
    scan_error_flag = 1;
    LOG.println(F("SCAN_ERROR"));
    delay(SCAN_DELAY);
  }
}

/* monitoreo
   ========= */

void listen() {
  // ¿el canal es valido?
  if (wbr.getRSQ()) {
    tune_timer = millis();
  } else {
    if (millis() - tune_timer > TUNE_LOST_DELAY) {
      // reporte calidad de señal
#if SNR_RSSI_LOG
      LOG.print(F("SNR_RSSI_AVG,"));
      LOG.print(wbr_channels[tune_channel], DEC);
      LOG.print(F(","));
      LOG.print(snr_rssi_samples_count, DEC);
      LOG.print(F(","));
      LOG.print(rssi_avg, 2);
      LOG.print(F(","));
      LOG.println(snr_avg, 2);
#endif
      // pasar a estado escaneo
      state = SCAN;
      same_test_timer = 0;
      LOG.print(F("TUNE_LOST,"));
      LOG.println(wbr_channels[tune_channel], DEC);
      return;
    }
  }

  // monitoreo tono 1050 khz
  boolean asq_status = wbr.getASQ();
  if (asq_prev_status != asq_status) {
    if (asq_status) {
      same_reset();
      LOG.println(F("ASQ_ON"));
    } else {
      LOG.println(F("ASQ_OFF"));
    }
    asq_prev_status = asq_status;
    return;
  }

  // monitoreo mensajes same
  byte same_state = wbr.getSAMEState();
  if (same_prev_state != same_state) {
    switch (same_state) {
    case SAME_EOM_DET:
      // fin del mensaje
      LOG.println(F("SAME_EOM_DET"));
      // ¿se recibieron una o dos cabeceras?
      if (same_headers_count > 0) {
        same_message();
      } else {
        same_reset();
      }
      break;
    case SAME_PRE_DET:
      // preámbulo detectado
      same_timer = millis();
      LOG.println(F("SAME_PRE_DET"));
      break;
    case SAME_HDR_DET:
      // cabecera detectada
      same_timer = millis();
      LOG.println(F("SAME_HDR_DET"));
      break;
    case SAME_HDR_RDY:
      // cabecera lista
      same_timer = millis();
      same_headers_count++;
      LOG.print(F("SAME_HDR_RDY,"));
      LOG.println(same_headers_count);
      break;
    }
    // ¿se recibieron tres cabeceras?
    if (same_headers_count == 3) {
      same_message();
    }
    same_prev_state = same_state;
    return;
  }

  // timeout mensaje same
  if (same_timer && millis() - same_timer > SAME_TIMEOUT) {
    LOG.println(F("SAME_TIMEOUT"));
    // ¿se recibieron una o dos cabeceras?
    if (same_headers_count > 0) {
      same_message();
    } else {
      same_reset();
    }
  }

  // timeout prueba same
  if (same_test_timer && millis() - same_test_timer > SAME_TEST_TIMEOUT) {
    same_test_timer = 0;
    LOG.println(F("SAME_TEST_TIMEOUT"));
  }

  // monitoreo calidad de señal
#if SNR_RSSI_LOG
  if (millis() - snr_rssi_sample_timer > SNR_RSSI_SAMPLE_DELAY) {
    // muestreo
    unsigned int rssi = wbr.getRSSI();
    unsigned int snr = wbr.getSNR();
    // promedio móvil acumulativo
    rssi_avg += (rssi - rssi_avg) / (snr_rssi_samples_count + 1);
    snr_avg += (snr - snr_avg) / (snr_rssi_samples_count + 1);
    // reporte muestra
#if SNR_RSSI_VERBOSE
    LOG.print(F("SNR_RSSI_SAMPLE,"));
    LOG.print(wbr_channels[tune_channel], DEC);
    LOG.print(F(","));
    LOG.print(snr_rssi_samples_count, DEC);
    LOG.print(F(","));
    LOG.print(rssi, DEC);
    LOG.print(F(","));
    LOG.println(snr, DEC);
#endif
    // actualización timer y conteo
    snr_rssi_sample_timer = millis();
    snr_rssi_samples_count++;
  }
  if (snr_rssi_samples_count == SNR_RSSI_LOG_SAMPLES) {
    // reporte calidad de señal
    LOG.print(F("SNR_RSSI_AVG,"));
    LOG.print(wbr_channels[tune_channel], DEC);
    LOG.print(F(","));
    LOG.print(snr_rssi_samples_count, DEC);
    LOG.print(F(","));
    LOG.print(rssi_avg, 2);
    LOG.print(F(","));
    LOG.println(snr_avg, 2);
    // reinicio promedios y contador
    rssi_avg = 0;
    snr_avg = 0;
    snr_rssi_samples_count = 0;
  }
#endif
}

/* mensajes same
   ============= */

void same_message() {
  byte size = wbr.getSAMESize();
  // ¿mensaje vacío?
  if (size < 1) {
    LOG.println(F("SAME_EMPTY"));
    return;
  }

  // adquisición mensaje
  byte msg[size];
  wbr.getSAMEMessage(size, msg);
  same_reset();

  // reportamos mensaje
  LOG.print(F("SAME,"));
  for (byte i = 0; i < size; i++) {
    if (msg[i] > 31 && msg[i] < 127) {
      LOG.write(msg[i]);
    } else {
      LOG.print(F("."));
    }
  }
  LOG.println();

  // interpretación mensaje
  if (size >= 38) {
    if (msg[5] == 'E' &&
        msg[6] == 'Q' &&
        msg[7] == 'W') {
      // alerta sísmica
      same_alert_timer = millis();
      LOG.println(F("SAME_EQW"));
    } else if (msg[5] == 'R' &&
               msg[6] == 'W' &&
               msg[7] == 'T') {
      // prueba periódica
      same_test_timer = millis();
      LOG.println(F("SAME_RWT"));
    }
  }

  // publicación mensaje
#if YUN_TWITTER
  String url = WS_URL;
  url += "?secret=";
  url += WS_SECRET;
  url += "&same=";
  for (byte i = 0; i < size; i++) {
    url += (int) msg[i];
    url += ':';
  }
  http.getAsynchronously(url);
  LOG.print(F("URL,"));
  LOG.println(url);
#endif
}

void same_reset() {
  // reinicio buffer, timer y contador
  wbr.clearSAMEBuffer();
  same_timer = 0;
  same_headers_count = 0;
}

/* servicio al usuario
   =================== */

void service_user() {
  // integrador botón de usuario
  if (digitalRead(USER_BUTTON)) {
    if (user_button_integrator < USER_BUTTON_INT_MAX) {
      user_button_integrator++;
    }
  } else {
    if (user_button_integrator > 0) {
      user_button_integrator--;
    }
  }
  boolean user_button_state = user_button_prev_state;
  if (user_button_integrator == 0) {
    user_button_state = LOW;
  } else if (user_button_integrator >= USER_BUTTON_INT_MAX) {
    user_button_state = HIGH;
    user_button_integrator = USER_BUTTON_INT_MAX;
  }

  // monitoreo botón de usuario
  if (user_button_prev_state != user_button_state) {
    if (user_button_state == LOW) {
      user_test_timer = 0;
      last_user_button_push = millis();
    }
    user_button_prev_state = user_button_state;
  } else {
    if (user_button_state == LOW) {
      if (millis() - last_user_button_push > USER_BUTTON_TEST_DELAY) {
        user_test_timer = millis();
        last_user_button_push = 0;
      }
    }
  }

  // promedio móvil compensado del voltaje externo
  ext_power_analog_avg *= 1 - (float) EXT_POWER_A;
  ext_power_analog_avg += analogRead(EXT_POWER) * (float) EXT_POWER_A;

  // actualización
  if (millis() - last_update > UPDATE_DELAY) {
    update_state = !update_state;
    if (same_alert_timer || user_test_timer) {
      alert_user();
    } else {
      update_user();
    }
    last_update = millis();
  }
}

void alert_user() {
  // alerta usuario
  digitalWrite(SIGNAL_LED_RED, update_state);
  digitalWrite(SIGNAL_LED_GREEN, LOW);
  digitalWrite(POWER_LED_RED, !update_state);
  digitalWrite(POWER_LED_GREEN, LOW);
  digitalWrite(BUZZER, update_state);
  // alerta sísmica
  if (millis() - same_alert_timer > ALARM_TIME) {
    same_alert_timer = 0;
  }
  // prueba de usuario
  if (millis() - user_test_timer > TEST_TIME) {
    user_test_timer = 0;
  }
}

void update_user() {
  // buzzer apagado
  digitalWrite(BUZZER, LOW);

  // led señal
  if (state == SCAN) {
    // escaneo
    if (scan_error_flag) {
      // error
      digitalWrite(SIGNAL_LED_GREEN, LOW);
      digitalWrite(SIGNAL_LED_RED, HIGH);
    } else {
      // sintonizando por primera vez
      digitalWrite(SIGNAL_LED_GREEN, LOW);
      digitalWrite(SIGNAL_LED_RED, LOW);
    }
  } else {
    // monitoreo
    digitalWrite(SIGNAL_LED_RED, LOW);
    if (same_test_timer) {
      // prueba same vigente
      digitalWrite(SIGNAL_LED_GREEN, HIGH);
    } else {
      // no hay prueba same vigente
      digitalWrite(SIGNAL_LED_GREEN, update_state);
    }
  }

  // led energía
  volatile unsigned int milivolts = ext_power_analog_avg * (float) EXT_POWER_M;
  if (milivolts >= CHARGE_VOLTAGE) {
    // energía externa
    digitalWrite(POWER_LED_RED, LOW);
    digitalWrite(POWER_LED_GREEN, HIGH);
  } else {
    // batería
    digitalWrite(POWER_LED_RED, LOW);
    digitalWrite(POWER_LED_GREEN, !update_state);
  }
}
