#include "Wire.h"
#include <TimerOne.h>
#include <Si4707.h>
#include "SismoAlerta.h"

/* variables y objetos globales 
   ============================ */

// weather band radio
Si4707 wbr(SI4707_RESET);
const unsigned long wbr_channels[] = { WBR_CHANNELS };

// banderas y parámetros
boolean scan_error_flag = 0;
byte tune_channel = 0;
boolean asq_prev_status = 0;
byte same_prev_state = 0;
byte same_headers_count = 0;
enum fsm_states { SCAN, LISTEN } state = SCAN;

// timers
unsigned long tune_timer = 0;
unsigned long same_timer = 0;
unsigned long same_test_timer = 0;
volatile unsigned long user_test_timer = 0;
volatile unsigned long same_alert_timer = 0;

// servicio al usuario
volatile unsigned int user_button_integrator = 0;
volatile boolean user_button_prev_state = HIGH;
volatile unsigned long last_user_button_push = 0;
volatile unsigned int last_ext_power_sample = 0;
volatile unsigned long last_update = 0;
volatile boolean update_state = LOW;

/* configuración 
   ============= */

void setup() {
  Serial.begin(9600);
  Serial.println();

  // configuramos pines de entrada/salida
  pinMode(BUZZER, OUTPUT);
  pinMode(POWER_LED_RED, OUTPUT);
  pinMode(POWER_LED_GREEN, OUTPUT);
  pinMode(SIGNAL_LED_RED, OUTPUT);
  pinMode(SIGNAL_LED_GREEN, OUTPUT);
  pinMode(USER_BUTTON, INPUT_PULLUP);
  
  // autoprueba
  Serial.println(F("SELFTEST"));
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
  
  // inicializamos si4707
  if (wbr.begin()) {
    // iniciamos la interrupción de servicio al usuario
    last_ext_power_sample = analogRead(EXT_POWER);
    Timer1.initialize(SERVICE_USER_MICROSEC);
    Timer1.attachInterrupt(service_user);
    // configuramos si4707    
    wbr.setSNR(TUNE_MIN_SNR);
    wbr.setRSSI(TUNE_MIN_RSSI);
  } else {
    // error al inicializar si4707
    digitalWrite(POWER_LED_RED, HIGH);
    digitalWrite(POWER_LED_GREEN, LOW);
    digitalWrite(SIGNAL_LED_RED, LOW);
    digitalWrite(SIGNAL_LED_GREEN, LOW);
    Serial.println(F("ERROR"));
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
  Serial.println(F("SCAN_START"));

  // tomamos muestras del rssi y snr en todos los canales
  unsigned int scan_rssi_sum[WBR_CHANNELS_SIZE] = { 0 };
  unsigned int scan_snr_sum[WBR_CHANNELS_SIZE] = { 0 };
  for (byte s = 0; s < SCAN_SAMPLES; s++) {
    for (byte c = 0; c < WBR_CHANNELS_SIZE; c++) {
      wbr.setWBFrequency(wbr_channels[c]);
      delay(SCAN_SAMPLE_DELAY);
      unsigned int rssi = wbr.getRSSI();
      scan_rssi_sum[c] += rssi;      
      unsigned int snr = wbr.getSNR();
      scan_snr_sum[c] += snr;
    }
  }

  // calculamos rssi/snr promedio y encontramos el mejor canal
  byte scan_best_channel = 0;
  unsigned int scan_best_rssi = 0;
  unsigned int scan_best_snr = 0;
  for (byte c = 0; c < WBR_CHANNELS_SIZE; c++) {
    float scan_rssi_avg = scan_rssi_sum[c] / (float) SCAN_SAMPLES;
    float scan_snr_avg = scan_snr_sum[c] / (float) SCAN_SAMPLES;
    if (scan_rssi_avg > scan_best_rssi && scan_snr_avg > scan_best_snr) {
      scan_best_channel = c;
      scan_best_rssi = scan_rssi_avg;
      scan_best_snr = scan_snr_avg;
    }
    Serial.print(F("SCAN,"));
    Serial.print(wbr_channels[c], DEC);
    Serial.print(F(","));
    Serial.print(scan_rssi_avg, 2);
    Serial.print(F(","));
    Serial.println(scan_snr_avg, 2);    
  }

  if (scan_best_rssi > TUNE_MIN_RSSI && scan_best_snr > TUNE_MIN_SNR) {
    // si encontramos un canal lo sintonizamos y pasamos al estado monitoreo
    state = LISTEN;
    scan_error_flag = 0;
    tune_channel = scan_best_channel;
    wbr.setWBFrequency(wbr_channels[tune_channel]);
    Serial.print(F("SCAN_OK,"));
    Serial.println(wbr_channels[scan_best_channel], DEC);
  } else {
    // repetimos el escaneo
    scan_error_flag = 1;  // bandera para encender el red de señal en rojo
    Serial.println(F("SCAN_ERROR"));
    delay(SCAN_DELAY);
  }
}

/* monitoreo 
   ========= */

void listen() {
  // monitoreamos si el canal aun es valido
  if (wbr.getRSQ()) {
    tune_timer = millis();
  } else {
    if (millis() - tune_timer > TUNE_LOST_DELAY) {
      state = SCAN;      
      same_test_timer = 0;
      Serial.print(F("TUNE_LOST,"));
      Serial.println(wbr_channels[tune_channel], DEC);
      return;
    }
  }
  
  // monitoreo del tono 1050 khz
  boolean asq_status = wbr.getASQ();
  if (asq_prev_status != asq_status) {
    if (asq_status) {
      same_reset();
      Serial.println(F("ASQ_ON"));      
    } else {
      Serial.println(F("ASQ_OFF"));      
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
      same_reset();
      Serial.println(F("SAME_EOM_DET"));
      break;
    case SAME_PRE_DET:
      // preámbulo detectado
      same_timer = millis();
      Serial.println(F("SAME_PRE_DET"));
      break;
    case SAME_HDR_DET:
      // cabecera detectada
      same_timer = millis();
      Serial.println(F("SAME_HDR_DET"));
      break;
    case SAME_HDR_RDY:
      // cabecera lista
      same_timer = millis();
      same_headers_count++;
      Serial.print(F("SAME_HDR_RDY,"));
      Serial.println(same_headers_count);
      break;
    }
    // procesamos mensaje después de recibir tres cabeceras
    if (same_headers_count == 3) {
      same_message();
    }
    same_prev_state = same_state;
    return;
  }

  // timeout mensaje same
  if (same_timer && millis() - same_timer > SAME_TIMEOUT) {
    same_reset();
    Serial.println(F("SAME_TIMEOUT"));
  }
  
  // timeout prueba same
  if (same_test_timer && millis() - same_test_timer > SAME_TEST_TIMEOUT) {
    same_test_timer = 0;
    Serial.println(F("SAME_TEST_TIMEOUT"));    
  }
}

/* mensajes same 
   ============= */

void same_message() {
  // obtenemos mensaje
  byte size = wbr.getSAMESize();
  if (size < 1) {
    Serial.println(F("SAME_EMPTY"));
    return;
  }
  byte msg[size];
  wbr.getSAMEMessage(size, msg);
  same_reset();
  
  // interpretamos evento
  if (size >= 38) {
    if (msg[5] == 'E' &&
        msg[6] == 'Q' &&
        msg[7] == 'W') {
      // alerta sísmica
      same_alert_timer = millis();
      Serial.println(F("SAME_EQW"));
    } else if (msg[5] == 'R' &&
               msg[6] == 'W' &&
               msg[7] == 'T') {
      // prueba periódica
      same_test_timer = millis();
      Serial.println(F("SAME_RWT"));
    }
  }
  
  // reportamos mensaje
  Serial.print(F("SAME,"));
  for (byte i = 0; i < size; i++) {
    if (msg[i] > 31 && msg[i] < 127) {
      Serial.write(msg[i]);
    } else {
      Serial.print(".");
    }
  }
  Serial.println();
}

void same_reset() {
  // limpiamos buffer, timer y contador de cabeceras
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

  // procesamos botón de usuario
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

  // filtro voltaje externo
  unsigned int ext_power_sample = (float) EXT_POWER_K_REL * analogRead(EXT_POWER);
  ext_power_sample += (1 - (float) EXT_POWER_K_REL) * last_ext_power_sample;
  last_ext_power_sample = ext_power_sample;
  
  // actualizamos
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
  digitalWrite(SIGNAL_LED_RED, update_state);
  digitalWrite(SIGNAL_LED_GREEN, LOW);
  digitalWrite(POWER_LED_RED, !update_state);
  digitalWrite(POWER_LED_GREEN, LOW);
  digitalWrite(BUZZER, update_state);
  if (millis() - same_alert_timer > ALARM_TIME) {
    same_alert_timer = 0;
  }
  if (millis() - user_test_timer > TEST_TIME) {
    user_test_timer = 0;
  }
}

void update_user() {
  // buzzer apagado
  digitalWrite(BUZZER, LOW);
  
  // led señal
  if (state == SCAN) {
    // escaneando canales
    if (scan_error_flag) {
      // error en escaneo
      digitalWrite(SIGNAL_LED_GREEN, LOW);    
      digitalWrite(SIGNAL_LED_RED, HIGH);
    } else {
      // sintonizando por primera vez
      digitalWrite(SIGNAL_LED_GREEN, LOW);
      digitalWrite(SIGNAL_LED_RED, LOW);          
    }
  } else {
    // con canal sintonizado
    digitalWrite(SIGNAL_LED_RED, LOW);      
    if (same_test_timer) {
      // con prueba same vigente
      digitalWrite(SIGNAL_LED_GREEN, HIGH);
    } else {
      // sin prueba same vigente
      digitalWrite(SIGNAL_LED_GREEN, update_state);
    }
  }
  
  // led energía
  volatile unsigned int milivolts = last_ext_power_sample * (float) EXT_POWER_K_MV;
  if (milivolts >= CHARGE_VOLTAGE) {
    // con energía externa
    digitalWrite(POWER_LED_RED, LOW);
    digitalWrite(POWER_LED_GREEN, HIGH);
  } else {
    // sin energía externa
    digitalWrite(POWER_LED_RED, LOW);
    digitalWrite(POWER_LED_GREEN, !update_state);
  }
}
