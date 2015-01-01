/* entrada/salida
   --------------

   SI4707_RESET: pin reset de si4707
   USER_BUTTON: pin del botón de usuario
   BUZZER: pin del zumbador
   EXT_POWER: pin analógico para medir voltaje externo
   POWER_LED_RED: pin del led rojo de energía
   POWER_LED_GREEN: pin del led verde de energía
   SIGNAL_LED_RED: pin del led rojo de señal 
   SIGNAL_LED_GREEN: pin del led verde de señal */

#define SI4707_RESET 10
#define USER_BUTTON 5
#define BUZZER 12
#define EXT_POWER 0
#define POWER_LED_RED 6
#define POWER_LED_GREEN 7
#define SIGNAL_LED_RED 8
#define SIGNAL_LED_GREEN 9

/* parámetros de operación
   -----------------------
   
   SCAN_SAMPLES: muestras por canal para el escaneo
   SCAN_SAMPLE_DELAY: retardo para tomar muestra del canal (milisegundos)
   SCAN_DELAY: retardo entre escaneo de canales (milisegundos)
   TUNE_MIN_SNR: relación señal/ruido mínima (db)
   TUNE_MIN_RSSI: potencia mínima (db)
   TUNE_LOST_DELAY: retardo para declarar un canal perdido (milisegundos)
   ALARM_TIME: duración de la alarma (milisegundos)
   TEST_TIME: duración de la prueba de alarma (milisegundos)
   SERVICE_USER_FREQ: frecuencia de actualización al usuario (hertz)
   USER_BUTTON_DEBOUNCE: retardo antirebote del botón de usuario (milisegundos)
   USER_BUTTON_TEST_DELAY: retardo para ejecutar prueba (milisegundos)
   CHARGE_VOLTAGE: voltaje mínimo para cargar la batería interna (milivolts)
   EXT_POWER_K_REL: constante de confiabilidad de la muestra de voltaje externo (0 a 1)
   UPDATE_DELAY: retardo para actualizar al usuario (milisegundos) 
   SELFTEST_DELAY: retardo autoprueba (milisegundos) */

#define SCAN_SAMPLES 8
#define SCAN_SAMPLE_DELAY 200
#define SCAN_DELAY 1000
#define TUNE_MIN_SNR 2
#define TUNE_MIN_RSSI 1
#define TUNE_LOST_DELAY 300000 // 5 minutos
#define ALARM_TIME 60000 // 1 minuto
#define TEST_TIME 10000 // 10 segundos
#define SERVICE_USER_FREQ 20 // 50 milisegundos
#define USER_BUTTON_DEBOUNCE 100
#define USER_BUTTON_TEST_DELAY 3000 // 3 segundos
#define CHARGE_VOLTAGE 4400
#define EXT_POWER_K_REL 0.8
#define UPDATE_DELAY 500
#define SELFTEST_DELAY 1000

/* weather band radio
   ------------------

   WBR_CHANNELS: frecuencia de los canales de weather band radio (en khz)
   WBR_CHANNELS_SIZE: número de canales de weather band radio
*/

#define WBR_CHANNELS 162400,162425,162450,162475,162500,162525,162550
#define WBR_CHANNELS_SIZE 7

/* constantes
   ---------- */

#define SAME_EOM_DET 0
#define SAME_PRE_DET 1
#define SAME_HDR_DET 2
#define SAME_HDR_RDY 3
#define SAME_TIMEOUT 6000
#define SAME_TEST_TIMEOUT 10800000 // 3 horas
#define SERVICE_USER_MICROSEC 1000000 / SERVICE_USER_FREQ
#define USER_BUTTON_INT_MAX ((USER_BUTTON_DEBOUNCE * SERVICE_USER_FREQ) / 1000)
#define EXT_POWER_K_MV (3300 / 512)
