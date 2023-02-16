
// Stack size needed to run SSH and the command parser.
const unsigned int configSTACK = 51200;

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libssh/priv.h"
#include <libssh/libssh.h>
#include "libssh_esp32.h"
#include "libssh_esp32_config.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"

//own header
#include "pwrmgr.h"
#include "ssh.h"
#include "sspiffs.h"
#include "wifi.h"

#define Red_LED   3    // rgb led pins, active low
#define Green_LED 4
#define Blue_LED  5
#define LED      21     // onboard blue led

volatile bool wifiPhyConnected;

// Timing and timeout configuration.
#define WIFI_TIMEOUT_S 10
#define NET_WAIT_MS 100


// Networking state of this esp32 device.
typedef enum
{
  STATE_NEW,
  STATE_PHY_CONNECTED,
  STATE_WAIT_IPADDR,
  STATE_GOT_IPADDR,
  STATE_CONNECT,
  STATE_DONE
} devState_t;

static volatile devState_t devState;

#define newDevState(s) (devState = s)

esp_err_t event_cb(void *ctx, system_event_t *event)
{
  switch(event->event_id)
  {
    case SYSTEM_EVENT_STA_START:
            printf("SYSTEM_EVENT_STA_START\n");
      esp_wifi_connect();
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
         printf("SYSTEM_EVENT_STA_CONNECTED\n");
      wifiPhyConnected = true;
      if (devState < STATE_PHY_CONNECTED) newDevState(STATE_PHY_CONNECTED);
      break;
    case SYSTEM_EVENT_GOT_IP6:
    case SYSTEM_EVENT_STA_GOT_IP:
        printf("got ip\n");
        newDevState(STATE_GOT_IPADDR);
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      if (devState < STATE_WAIT_IPADDR) newDevState(STATE_NEW);
      if (wifiPhyConnected)
      {
        wifiPhyConnected = false;
      }
      wifi_connect();
      break;
    default:
      break;
  }
  return ESP_OK;
}


void controlTask(void *pvParameter)
{

  doorCMD_t state = *(doorCMD_t *) pvParameter;

  if (ESP_OK == initSPIFFS()){
    printf("sspifs init ok\n");
    printSPIFFSinfo();
  }
  else{
    printf("sspifs init failed\n");//no files, no key, just give up
    enterDeepsleep();
  }

  wifiPhyConnected = false;
  wifi_connect();

  while (1)
  {
    switch (devState)
    {
      case STATE_NEW :
    
         printf("%%STATE_NEW\n");
        vTaskDelay(NET_WAIT_MS / portTICK_PERIOD_MS);
        break;
      case STATE_PHY_CONNECTED :
        printf("%%STATE_PHY_CONNECTED\n");
        newDevState(STATE_WAIT_IPADDR);
        break;
      case STATE_WAIT_IPADDR :
        vTaskDelay(NET_WAIT_MS / portTICK_PERIOD_MS);
        break;
      case STATE_GOT_IPADDR :
            printf("%%STATE_GOT_IPADDR\n");
        newDevState(STATE_CONNECT);
        break;
      case STATE_CONNECT :
        printf("%%STATE_CONNECT\n");
        libssh_begin();

        {
          int ex_rc = ex_main(state);
          printf("\n%% Execution completed: rc=%d\n", ex_rc);
        }
        newDevState(STATE_DONE);
      case STATE_DONE :
          enterDeepsleep();
      default:
        break;
    }
  }

  printf("this should never happen.\n");
  vTaskDelete(NULL);
}


void loop(void *pvParameter){
 while (1){
    if (esp_timer_get_time() > 30*1000*1000){//uptime > 30 sec 
      
      printflashinfo();
      printf("took too long\n");
      enterDeepsleep();
    } 
    vTaskDelay(100 / portTICK_PERIOD_MS);
    printf(".");
    }
}

extern "C" void app_main() {
  devState = STATE_NEW;
  // Use the expected blocking I/O behavior.
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_3, 0); //set red led on

  uart_driver_install ((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

  xTaskCreate(loop, "loop", 1024, NULL, (tskIDLE_PRIORITY + 3) , NULL);

  printf(esp_get_idf_version());


  doorCMD_t shackCMD = wakeSelector();

  switch (shackCMD){
    case CMD_UNKNOWN:   printf("UNKNOWN!\n");  enterDeepsleep();  break;
    case CMD_OPEN:      printf("OPEN!\n");   break;
    case CMD_CLOSE:     printf("CLOSE!\n");   break;
    default:  break;
  }

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      printf("nvs_flash_init failed");
  }

 // esp_netif_init();
  tcpip_adapter_init();

  esp_event_loop_init(event_cb, NULL);
  xTaskCreatePinnedToCore(controlTask, "ctl", configSTACK, (void *) &shackCMD , (tskIDLE_PRIORITY + 3), NULL, portNUM_PROCESSORS - 1);

  while (1){
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}