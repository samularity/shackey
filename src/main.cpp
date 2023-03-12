
// Stack size needed to run SSH and the command parser.
const unsigned int configSTACK = 51200;

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
//own header
#include "pwrmgr.h"
#include "ssh.h"
#include "sspiffs.h"
#include "wifi.h"
#include "webserver.h"
#include "dns_server.h"

#define Red_LED   3    // rgb led pins, active low
#define Green_LED 4
#define Blue_LED  5
#define LED      21     // onboard blue led


EventGroupHandle_t s_wifi_event_group;


EventGroupHandle_t xEventGroup;

int TASK_FINISH_BIT	= BIT4;

void loop(void *pvParameter){

  EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);

  while (1){
    vTaskDelay(100 / portTICK_PERIOD_MS);
    bits = xEventGroupGetBits(s_wifi_event_group);
    if ( (bits & WIFI_CLIENT_CONNECTED_BIT) ){
      printf("stay awake forever\n");
      break;
    }
    if ((esp_timer_get_time() > 45*1000*1000)){//uptime > 30 sec 
      printf("took too long\n");
      enterDeepsleep();
    } 
  }
  vTaskDelete( NULL ); //exit
}


void SetupMode (void *pvParameter){

  //esp_log_level_set("wifi", ESP_LOG_DEBUG);

  wifi_init_softap();
  start_webserver();
  start_dns_server();

  while (1){
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}


extern "C" void app_main() {

  sshParameter cfg = {
    .username = "close",
    .hostname = "portal.shackspace.de",
    .port = 22,
    .password = {'\0'},
    .publickey = "/spiffs/key.pub",
    .privatekey = "/spiffs/key",
    .cmd = {'\0'},
  };

  // Use the expected blocking I/O behavior.
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  //GPIO-INIT
  gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_3, 0); //set red led on

  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT);

  gpio_set_pull_mode (GPIO_NUM_2,GPIO_PULLDOWN_ONLY);
  gpio_set_pull_mode (GPIO_NUM_2,GPIO_PULLDOWN_ONLY);

  uart_driver_install ((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

//vTaskDelay(4000 / portTICK_PERIOD_MS);
printf("starting...\n");

  esp_log_level_set("SSH", ESP_LOG_INFO);

  printf("%s:%d: %llums\n",__FILE__ , __LINE__ , (uint64_t) (esp_timer_get_time()/1000) );


  s_wifi_event_group = xEventGroupCreate();
  xEventGroupClearBits( s_wifi_event_group, WIFI_CONNECTED_BIT || WIFI_FAIL_BIT || WIFI_CLIENT_CONNECTED_BIT );

  xTaskCreate(loop, "loop", 1024, NULL, (tskIDLE_PRIORITY + 1) , NULL);

  if (ESP_OK == initSPIFFS()){
    printf("sspifs init ok\n");
    printSPIFFSinfo();
  }
  else{
    printf("sspifs init failed\n");//no files, no key, just give up
    enterDeepsleep();
  }

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      printf("nvs_flash_init failed");
  }

  switch (wakeSelector()){
    case CMD_OPEN:  strcpy(cfg.username , "open-front");  break;
    case CMD_CLOSE: strcpy(cfg.username, "close");        break;
    default:          
      printf("UNKNOWN!\n");
      if (1 == gpio_get_level(GPIO_NUM_2)){
        xTaskCreate(SetupMode, "setup", 1024*8, NULL, (tskIDLE_PRIORITY + 1) , NULL); 
        while (1){ vTaskDelay(1000 / portTICK_PERIOD_MS); } //idle forever
      }
      enterDeepsleep(); 
      break;
  }

  esp_netif_init();
  wifi_init_sta();

  /* Wait for Wi-Fi connection */
  xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

  printf("starting ssh things\n");

  xEventGroup = xEventGroupCreate();// Create Eventgroup for ssh response
  xEventGroupClearBits( xEventGroup, TASK_FINISH_BIT );
      
  xTaskCreatePinnedToCore(&ssh_task, "ssh", 1024*8, (void *) &cfg , tskIDLE_PRIORITY , NULL, portNUM_PROCESSORS - 1);

  // Wit for ssh finish.
  xEventGroupWaitBits( xEventGroup,
    TASK_FINISH_BIT,	/* The bits within the event group to wait for. */
    pdTRUE,				/* HTTP_CLOSE_BIT should be cleared before returning. */
    pdFALSE,			/* Don't wait for both bits, either bit will do. */
    portMAX_DELAY);		/* Wait forever. */


  printf("ssh done, sleep\n");
  printf("uptime: %llums\n", (uint64_t) (esp_timer_get_time()/1000) );
  enterDeepsleep();

  while (1){
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}