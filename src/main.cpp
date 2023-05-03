
// Stack size needed to run SSH and the command parser.
const unsigned int configSTACK = 51200;

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
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

/*
| PIN-Number | ESP-Pin Name | Description  |
|----|----------------------|--------------|
|  4 | GPIO0 / XTAL_32K_P   | Enable ADC   |
| 27 | GPIO20 / U0RXD       | ADC          |
|  9 | GPIO4 / MTMS         | SW1 Close (High when pressed) |
| 10 | GPIO5 / MTDI         | SW2 Open (High pressed) |
| 16 | GPIO10               | LED1 Green   |
| 12 | GPIO6 / MTCK         | LED1 Red     |
| 13 | GPIO7 / MTDO         | LED1 Blue    |
| 18 | GPIO11 / VDD_SPI     | LED2 Green   |
|  8 | GPIO3                | LED2 Red     |
|  5 | GPIO1 / XTAL_32K_N   | LED2 Blue    |
*/
#define GPIO_ADC_ENABLE GPIO_NUM_0
#define GPIO_ADC_READ GPIO_NUM_20

#define GPIO_SW_CLOSE GPIO_NUM_4
#define GPIO_SW_OPEN GPIO_NUM_5

#define GPIO_LED1_RED GPIO_NUM_6
#define GPIO_LED1_GREEN GPIO_NUM_10
#define GPIO_LED1_BLUE GPIO_NUM_7

#define GPIO_LED2_RED GPIO_NUM_3
#define GPIO_LED2_GREEN GPIO_NUM_11 //DOES NOT WORK as GPIO, RESERVED for VDD_SPI
#define GPIO_LED2_BLUE GPIO_NUM_1

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (1000) // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095  max 8191
#define LEDC_FREQUENCY          (5000) // Frequency in Hertz. Set frequency at 5 kHz



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

  //configure inputs
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pin_bit_mask = ( (1ULL<<GPIO_SW_CLOSE) | (1ULL<<GPIO_SW_OPEN) );
	io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = (1ULL<<GPIO_ADC_READ);
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpio_config(&io_conf);

  //configure outputs
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = ( (1ULL<<GPIO_ADC_ENABLE) |  (1ULL<<GPIO_LED1_RED) | (1ULL<<GPIO_LED1_GREEN) | (1ULL<<GPIO_LED1_BLUE) | (1ULL<<GPIO_LED2_RED) | (1ULL<<GPIO_LED2_BLUE)  );
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&io_conf);

//Disable all LEDs

  gpio_set_level(GPIO_LED2_BLUE, 1);
  gpio_set_level(GPIO_LED1_RED, 1);
  gpio_set_level(GPIO_LED1_GREEN, 1);
  gpio_set_level(GPIO_LED1_BLUE, 1);
  gpio_set_level(GPIO_LED2_RED, 1);
	gpio_set_level(GPIO_LED2_GREEN, 1);


  uart_driver_install ((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

 // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = GPIO_LED1_GREEN,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };

  ledc_channel_config(&ledc_channel);

  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 450);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

  gpio_set_level(GPIO_LED1_RED, 0);
  gpio_set_level(GPIO_LED2_RED, 0);

  printf("starting...\n");

  esp_log_level_set("SSH", ESP_LOG_DEBUG);

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
      case CMD_OPEN:  strcpy(cfg.username , "open-front");  printf("ssh cmd open\n"); break;
      case CMD_CLOSE: strcpy(cfg.username, "close");  printf("ssh cmd stop\n");   break;
      default:     
        printf("UNKNOWN!\n");
          gpio_set_level(GPIO_LED2_BLUE, 0);
        if (1 == gpio_get_level(GPIO_SW_OPEN)){
          xTaskCreate(SetupMode, "setup", 1024*8, NULL, (tskIDLE_PRIORITY + 1) , NULL); 
          while(1) {vTaskDelay(10000 / portTICK_PERIOD_MS);}  //idle forever
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