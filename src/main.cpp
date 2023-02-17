
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
//own header
#include "pwrmgr.h"
#include "ssh.h"
#include "sspiffs.h"
#include "wifi.h"

#define Red_LED   3    // rgb led pins, active low
#define Green_LED 4
#define Blue_LED  5
#define LED      21     // onboard blue led


EventGroupHandle_t s_wifi_event_group;


EventGroupHandle_t xEventGroup;
int TASK_FINISH_BIT	= BIT4;

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

  // Use the expected blocking I/O behavior.
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_3, 0); //set red led on

  uart_driver_install ((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

  printf("%s:%d: %llums\n",__FILE__ , __LINE__ , (uint64_t) (esp_timer_get_time()/1000) );

  xTaskCreate(loop, "loop", 1024, NULL, (tskIDLE_PRIORITY + 1) , NULL);

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

  esp_netif_init();
  wifi_init_sta();

  if (ESP_OK == initSPIFFS()){
    printf("sspifs init ok\n");
    printSPIFFSinfo();
  }
  else{
    printf("sspifs init failed\n");//no files, no key, just give up
    enterDeepsleep();
  }

  /* Wait for Wi-Fi connection */
  xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

  printf("starting ssh things\n");

        
    char line[256] = "date";//ssh command to execute
    xEventGroup = xEventGroupCreate();// Create Eventgroup for ssh response
    xEventGroupClearBits( xEventGroup, TASK_FINISH_BIT );
          
          //xTaskCreate(&ssh_task, "SSH", 1024*8, (void *) &line, 2, NULL);
    xTaskCreatePinnedToCore(&ssh_task, "ssh", 1024*8, (void *) &line , (tskIDLE_PRIORITY + 3), NULL, portNUM_PROCESSORS - 1);

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