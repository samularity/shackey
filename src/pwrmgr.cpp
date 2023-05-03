#include "pwrmgr.h"
#include "esp_sleep.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include <inttypes.h>

doorCMD_t wakeSelector(){
  doorCMD_t res = CMD_UNKNOWN;
  u_int64_t wakeMask = esp_sleep_get_gpio_wakeup_status();

  //printf("GPIO that triggered the wake up: GPIO %hhu \n",  (uint8_t)((log(wakeMask))/log(2), 0) );

  if (wakeMask == 0b100000 ){
    res = CMD_OPEN;
  }
  else if (wakeMask == 0b010000){
    res = CMD_CLOSE;
  }

  return res;
}


void enterDeepsleep(){
  //https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/gpio.html
  //Only RTC IO can be used as a source for external wake
  //source. They are pins: 0 to 5
  //0b110000 = bitmask GPIO4, GPIO5

  esp_deep_sleep_enable_gpio_wakeup(0b110000, ESP_GPIO_WAKEUP_GPIO_HIGH);
  printf("Going to deep-sleep now\n");
  //vTaskDelay(100 / portTICK_PERIOD_MS);

  esp_deep_sleep_start();
}


void printflashinfo() {

  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }
  printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
          (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
  
}