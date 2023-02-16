#ifndef _WIFI_H_
#define _WIFI_H_

#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
/* FreeRTOS event group to signal when we are connected*/
//static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void wifi_init_sta(void);

void wifi_connect();


#endif /* _WIFI_H_ */