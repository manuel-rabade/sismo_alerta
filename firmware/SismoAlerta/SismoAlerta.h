/* entrada/salida
   --------------

   SI4707_RESET: pin reset si4707
   USER_BUTTON: pin botón usuario
   BUZZER: pin zumbador
   EXT_POWER: pin analógico voltaje externo
   POWER_LED_RED: pin led rojo energía
   POWER_LED_GREEN: pin led verde energía
   SIGNAL_LED_RED: pin led rojo señal
   SIGNAL_LED_GREEN: pin led verde señal */

#define SI4707_RESET 10
#define SI4707_ENABLE 11 // obsoleto
#define USER_BUTTON 5
#define BUZZER 12
#define EXT_POWER 0
#define POWER_LED_RED 6
#define POWER_LED_GREEN 7
#define SIGNAL_LED_RED 8
#define SIGNAL_LED_GREEN 9

/* parámetros de operación
   -----------------------

   SCAN_TIMES: numero de escaneos
   SCAN_DELAY: retardo entre escaneos (milisegundos)
   SCAN_SAMPLES: muestras por canal por escaneo
   SCAN_SAMPLE_DELAY: retardo para muestrear canal (milisegundos)
   SCAN_VERBOSE: informar cada muestra del escaneo (0 o 1)
   TUNE_MIN_RSSI: potencia mínima (db)
   TUNE_MIN_SNR: relación señal/ruido mínima (db)
   TUNE_LOST_DELAY: retardo para declarar un canal perdido (milisegundos)
   ALARM_TIME: duración alarma (milisegundos)
   TEST_TIME: duración prueba de alarma (milisegundos)
   SERVICE_USER_FREQ: frecuencia de actualización al usuario (hertz)
   USER_BUTTON_DEBOUNCE: retardo antirebote del botón de usuario (milisegundos)
   USER_BUTTON_TEST_DELAY: retardo para ejecutar prueba de usuario (milisegundos)
   CHARGE_VOLTAGE: voltaje mínimo para cargar la batería interna (milivolts)
   EXT_POWER_A: constante para el promedio móvil del voltaje externo (0 a 1)
   UPDATE_DELAY: retardo para actualizar al usuario (milisegundos)
   SELFTEST_DELAY: retardo autoprueba (milisegundos)
   SNR_RSSI_LOG: monitoreo calidad de señal (0 o 1)
   SNR_RSSI_LOG_SAMPLES: muestras de calidad de señal a promediar
   SNR_RSSI_SAMPLE_DELAY: retardo para muestrear la calidad de señal (milisegundos)
   SNR_RSSI_VERBOSE: informar cada muestra de calidad de señal (0 o 1) */

#define SCAN_TIMES 3
#define SCAN_DELAY 1000
#define SCAN_SAMPLES 3
#define SCAN_SAMPLE_DELAY 300
#define SCAN_VERBOSE 0
#define TUNE_MIN_RSSI 2
#define TUNE_MIN_SNR 5
#define TUNE_LOST_DELAY 300000 // 5 minutos
#define ALARM_TIME 60000 // 1 minuto
#define TEST_TIME 10000 // 10 segundos
#define SERVICE_USER_FREQ 20 // 50 milisegundos
#define USER_BUTTON_DEBOUNCE 100
#define USER_BUTTON_TEST_DELAY 3000 // 3 segundos
#define CHARGE_VOLTAGE 3750
#define EXT_POWER_A 0.70
#define UPDATE_DELAY 500
#define SELFTEST_DELAY 1000
#define SNR_RSSI_LOG 1
#define SNR_RSSI_LOG_SAMPLES 240 // 20 minutos
#define SNR_RSSI_SAMPLE_DELAY 5000 // 5 segundos
#define SNR_RSSI_VERBOSE 0

/* weather band radio
   ------------------

   WBR_CHANNELS: frecuencia de los canales de weather band radio (en khz)
   WBR_CHANNELS_SIZE: número de canales de weather band radio */

#define WBR_CHANNELS 162400,162425,162450,162475,162500,162525,162550
#define WBR_CHANNELS_SIZE 7

/* monitoreo
   ---------

   YUN_LOGGER: modo arduino yún con bitácora (0 o 1)
   YUN_TWITTER: modo arduino yún con retransmisor twitter (0 o 1) */

#define YUN_LOGGER 0
#define YUN_TWITTER 0

/* consola
   ------- */

#if (YUN_LOGGER || YUN_TWITTER)
#define LOG Console
#else
#define LOG Serial
#endif

/* constantes
   ---------- */

#define SAME_EOM_DET 0
#define SAME_PRE_DET 1
#define SAME_HDR_DET 2
#define SAME_HDR_RDY 3
#define SAME_TIMEOUT 6000
#define SAME_TEST_TIMEOUT 11400000 // 3 horas y 10 minutos
#define SERVICE_USER_MICROSEC (1000000 / SERVICE_USER_FREQ)
#define USER_BUTTON_INT_MAX ((USER_BUTTON_DEBOUNCE * SERVICE_USER_FREQ) / 1000)
#define EXT_POWER_M (3300 / 512)
