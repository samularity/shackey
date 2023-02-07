
#include "esp_sleep.h"

// ESP32 libssh port.
//
// Ewan Parker, created 22nd April 2020.
// Simple port of examples/template.c over WiFi.  Run with a serial monitor at
// 115200 BAUD.
//
// Copyright (C) 2016–2022 Ewan Parker.

// Set local WiFi credentials below.
const char *configSTASSID = "Portal-Dev";
const char *configSTAPSK = NULL;
//const char *configSTAPSK = "53533357810010669267";

// Stack size needed to run SSH and the command parser.
const unsigned int configSTACK = 51200;

#include "IPv6Address.h"
#include "WiFi.h"
// Include the Arduino library.
#include "libssh_esp32.h"

// EXAMPLE includes/defines START
#include <libssh/libssh.h>
#include "common.h"
// EXAMPLE includes/defines FINISH

volatile bool wifiPhyConnected;

// Timing and timeout configuration.
#define WIFI_TIMEOUT_S 10
#define NET_WAIT_MS 100

typedef enum
{
  CMD_UNKNOWN,
  CMD_OPEN,
  CMD_CLOSE,
} doorCMD_t; 

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
static volatile bool gotIpAddr, gotIp6Addr;

#include "SPIFFS.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"

// EXAMPLE functions START
/*
 * knownhosts.c
 * This file contains an example of how verify the identity of a
 * SSH server using libssh
 */

/*
Copyright 2003-2009 Aris Adamantiadis

This file is part of the SSH Library

You are free to copy this file, modify it in any way, consider it being public
domain. This does not apply to the rest of the library though, but it is
allowed to cut-and-paste working code from this file to any license of
program.
The goal is to show the API in action. It's not a reference on how terminal
clients must be made or how a client should react.
 */

#include "libssh_esp32_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libssh/priv.h"
#include <libssh/libssh.h>
#include "common.h"


int verify_knownhost(ssh_session session)
{
    enum ssh_known_hosts_e state;
    char buf[10];
    unsigned char *hash = NULL;
    size_t hlen;
    ssh_key srv_pubkey;
    int rc;

    rc = ssh_get_server_publickey(session, &srv_pubkey);
    if (rc < 0) {
        return -1;
    }

    rc = ssh_get_publickey_hash(srv_pubkey,
                                SSH_PUBLICKEY_HASH_SHA256,
                                &hash,
                                &hlen);
    ssh_key_free(srv_pubkey);
    if (rc < 0) {
        return -1;
    }

    state = ssh_session_is_known_server(session);

    switch(state) {
    case SSH_KNOWN_HOSTS_CHANGED:
        fprintf(stderr,"Host key for server changed : server's one is now :\n");
        ssh_print_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);
        ssh_clean_pubkey_hash(&hash);
        fprintf(stderr,"For security reason, connection will be stopped\n");
        return -1;
    case SSH_KNOWN_HOSTS_OTHER:
        fprintf(stderr,"The host key for this server was not found but an other type of key exists.\n");
        fprintf(stderr,"An attacker might change the default server key to confuse your client"
                "into thinking the key does not exist\n"
                "We advise you to rerun the client with -d or -r for more safety.\n");
        return -1;
    case SSH_KNOWN_HOSTS_NOT_FOUND:
        fprintf(stderr,"Could not find known host file. If you accept the host key here,\n");
        fprintf(stderr,"the file will be automatically created.\n");
        /* fallback to SSH_SERVER_NOT_KNOWN behavior */
        FALL_THROUGH;
    case SSH_SERVER_NOT_KNOWN:
        fprintf(stderr,
                "The server is unknown. Do you trust the host key (yes/no)?\n");
        ssh_print_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hlen);

        /*
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            ssh_clean_pubkey_hash(&hash);
            return -1;
        }
        */
        buf[0]='y';
        buf[1]='e';
        buf[2]='s';

        if(strncasecmp(buf,"yes",3)!=0){
            ssh_clean_pubkey_hash(&hash);
            return -1;
        }
        fprintf(stderr,"This new key will be written on disk for further usage. do you agree ?\n");
        /*
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            ssh_clean_pubkey_hash(&hash);
            return -1;
        }
        */
        buf[0]='y';
        buf[1]='e';
        buf[2]='s';

        if(strncasecmp(buf,"yes",3)==0){
            rc = ssh_session_update_known_hosts(session);
            if (rc != SSH_OK) {
                ssh_clean_pubkey_hash(&hash);
                fprintf(stderr, "error %s\n", strerror(errno));
                return -1;
            }
        }

        break;
    case SSH_KNOWN_HOSTS_ERROR:
        ssh_clean_pubkey_hash(&hash);
        fprintf(stderr,"%s",ssh_get_error(session));
        return -1;
    case SSH_KNOWN_HOSTS_OK:
        break; /* ok */
    }

    ssh_clean_pubkey_hash(&hash);

    return 0;
}

/*
 * authentication.c
 * This file contains an example of how to do an authentication to a
 * SSH server using libssh
 */

/*
Copyright 2003-2009 Aris Adamantiadis

This file is part of the SSH Library

You are free to copy this file, modify it in any way, consider it being public
domain. This does not apply to the rest of the library though, but it is
allowed to cut-and-paste working code from this file to any license of
program.
The goal is to show the API in action. It's not a reference on how terminal
clients must be made or how a client should react.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libssh/libssh.h>
#include "common.h"



static int auth_keyfile(ssh_session session, char* keyfile)
{
    ssh_key key = NULL;
    char pubkey[132] = {0}; // +".pub"
    int rc;

    snprintf(pubkey, sizeof(pubkey), "%s.pub", keyfile);

    rc = ssh_pki_import_pubkey_file( pubkey, &key);

    if (rc != SSH_OK)
        return SSH_AUTH_DENIED;

    rc = ssh_userauth_try_publickey(session, NULL, key);

    ssh_key_free(key);

    if (rc!=SSH_AUTH_SUCCESS)
        return SSH_AUTH_DENIED;

    rc = ssh_pki_import_privkey_file(keyfile, NULL, NULL, NULL, &key);

    if (rc != SSH_OK)
        return SSH_AUTH_DENIED;
    rc = ssh_userauth_publickey(session, NULL, key);
    ssh_key_free(key);

    return rc;
}


static void error(ssh_session session)
{
    fprintf(stderr,"Authentication failed: %s\n",ssh_get_error(session));
}

int authenticate_console_shack(ssh_session session)
{
    int rc;
    char *banner;

    char fname[64] = {"/spiffs/key"};

    rc = auth_keyfile(session, fname);

    if(rc == SSH_AUTH_SUCCESS) {
      banner = ssh_get_issue_banner(session);
      if (banner) {
          printf("%s\n",banner);
          SSH_STRING_FREE_CHAR(banner);
      }
    }
    else{
      printf("SSH_AUTH_FAILED\n");
    }

    return rc;
}

/*
 * connect_ssh.c
 * This file contains an example of how to connect to a
 * SSH server using libssh
 */

/*
Copyright 2009 Aris Adamantiadis

This file is part of the SSH Library

You are free to copy this file, modify it in any way, consider it being public
domain. This does not apply to the rest of the library though, but it is
allowed to cut-and-paste working code from this file to any license of
program.
The goal is to show the API in action. It's not a reference on how terminal
clients must be made or how a client should react.
 */

#include <libssh/libssh.h>
#include "common.h"
#include <stdio.h>

ssh_session connect_ssh(const char *host, const char *user,int verbosity){
  ssh_session session;
  int auth=0;

  session=ssh_new();
  if (session == NULL) {
    return NULL;
  }

  if(user != NULL){
    if (ssh_options_set(session, SSH_OPTIONS_USER, user) < 0) {
      ssh_free(session);
      return NULL;
    }
  }

  if (ssh_options_set(session, SSH_OPTIONS_HOST, host) < 0) {
    ssh_free(session);
    return NULL;
  }
  ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
  if(ssh_connect(session)){
    fprintf(stderr,"Connection failed : %s\n",ssh_get_error(session));
    ssh_disconnect(session);
    ssh_free(session);
    return NULL;
  }
  /*
  if(verify_knownhost(session)<0){
    ssh_disconnect(session);
    ssh_free(session);
    return NULL;
  }
  */
  auth=authenticate_console_shack(session);
  if(auth==SSH_AUTH_SUCCESS){
    printf("ssh auth ok\n");
    return session;
  } else if(auth==SSH_AUTH_DENIED){
    fprintf(stderr,"ssh auth err\n");
  } else {
    fprintf(stderr,"Error while authenticating : %s\n",ssh_get_error(session));
  }
  ssh_disconnect(session);
  ssh_free(session);
  return NULL;
}
// EXAMPLE functions FINISH

// EXAMPLE main START
int ex_main(doorCMD_t shackstate){
    ssh_session session;
    ssh_channel channel;
    char buffer[256];
    int rbytes, wbytes, total = 0;
    int rc;

    if (CMD_OPEN == shackstate ){
        //session = connect_ssh("portal.shackspace.de", "open-front", 0xffffU);//get all logs
        session = connect_ssh("portal.shackspace.de", "open-front", 0);//get all logs
    } 
    else if (CMD_CLOSE == shackstate )
    {
      //session = connect_ssh("portal.shackspace.de", "close", 0xffffU);//get all logs
      session = connect_ssh("portal.shackspace.de", "close", 0);//get all logs
    }
    
    if (session == NULL) {
        ssh_finalize();
        return 1;
    }

    channel = ssh_channel_new(session);
    if (channel == NULL) {
        ssh_disconnect(session);
        ssh_free(session);
        ssh_finalize();
        return 1;
    }

    rc = ssh_channel_open_session(channel);
    if (rc < 0) {
        goto failed;
    }


    rc = ssh_channel_request_shell (channel);
    if (rc < 0) {
        goto failed;
    }

/*
    rc = ssh_channel_request_exec(channel, "date");
    if (rc < 0) {
        goto failed;
    }
*/

    rbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
    if (rbytes <= 0) {
        goto failed;
    }

    do {
        wbytes = fwrite(buffer + total, 1, rbytes, stdout);
        if (wbytes <= 0) {
            goto failed;
        }

        total += wbytes;

        // When it was not possible to write the whole buffer to stdout 
        if (wbytes < rbytes) {
            rbytes -= wbytes;
            continue;
        }

        rbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
        total = 0;
    } while (rbytes > 0);

    if (rbytes < 0) {
        goto failed;
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    ssh_finalize();

    return 0;
failed:
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    ssh_finalize();

    return 1;
}
// EXAMPLE main FINISH

#define newDevState(s) (devState = s)
esp_err_t event_cb(void *ctx, system_event_t *event)
{
  switch(event->event_id)
  {
    case SYSTEM_EVENT_STA_START:
      Serial.print("% WiFi enabled with SSID=");
      Serial.println(configSTASSID);
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      WiFi.enableIpV6();
      wifiPhyConnected = true;
      if (devState < STATE_PHY_CONNECTED) newDevState(STATE_PHY_CONNECTED);
      break;
    case SYSTEM_EVENT_GOT_IP6:
      if (event->event_info.got_ip6.ip6_info.ip.addr[0] != htons(0xFE80)
      && !gotIp6Addr)
      {
        gotIp6Addr = true;
      }
      Serial.print("% IPv6 Address: ");
      Serial.println(IPv6Address(event->event_info.got_ip6.ip6_info.ip.addr));
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      gotIpAddr = true;
      Serial.print("% IPv4 Address: ");
      Serial.println(IPAddress(event->event_info.got_ip.ip_info.ip.addr));
      break;
    case SYSTEM_EVENT_STA_LOST_IP:
      //gotIpAddr = false;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      if (devState < STATE_WAIT_IPADDR) newDevState(STATE_NEW);
      if (wifiPhyConnected)
      {
        wifiPhyConnected = false;
      }
      WiFi.begin(configSTASSID, configSTAPSK);
      break;
    default:
      break;
  }
  return ESP_OK;
}

void enterDeepsleep(){
  //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html
  //Only RTC IO can be used as a source for external wake
  //source. They are pins: 0,2,4,12-15,25-27,32-39.
  //0b010100 = bitmask GPIO2, GPIO4

  esp_deep_sleep_enable_gpio_wakeup(0b010100, ESP_GPIO_WAKEUP_GPIO_HIGH);
  Serial.println("Going to deep-sleep now\n");
  Serial.flush(); 
  //vTaskDelay(100 / portTICK_PERIOD_MS);
  esp_deep_sleep_start();
}

void controlTask(void *pvParameter)
{

  doorCMD_t state = *(doorCMD_t *) pvParameter;


  // Mount the file system.
  boolean fsGood = SPIFFS.begin();
  if (!fsGood)
  {
    printf("%% No formatted SPIFFS filesystem found to mount.\n");
    printf("%% Format SPIFFS and mount now (NB. may cause data loss) [y/n]?\n");
    while (!Serial.available()) {}
    char c = Serial.read();
    if (c == 'y' || c == 'Y')
    {
      printf("%% Formatting...\n");
      fsGood = SPIFFS.format();
      if (fsGood) SPIFFS.begin();
    }
  }
  if (!fsGood)
  {
    printf("%% Aborting now.\n");
    while (1) vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
  printf(
    "%% Mounted SPIFFS used=%d total=%d\r\n", SPIFFS.usedBytes(),
    SPIFFS.totalBytes());

  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while(file){
      printf("\t/%s\n", file.name());
      file = root.openNextFile();
  }

  wifiPhyConnected = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_MODE_STA);
  gotIpAddr = false; gotIp6Addr = false;
  WiFi.begin(configSTASSID, configSTAPSK);

  TickType_t xStartTime;
  xStartTime = xTaskGetTickCount();
  const TickType_t xTicksTimeout = WIFI_TIMEOUT_S*1000/portTICK_PERIOD_MS;

  while (1)
  {
    switch (devState)
    {
      case STATE_NEW :
        vTaskDelay(NET_WAIT_MS / portTICK_PERIOD_MS);
        break;
      case STATE_PHY_CONNECTED :
        newDevState(STATE_WAIT_IPADDR);
        // Set the initial time, where timeout will be started
        xStartTime = xTaskGetTickCount();
        break;
      case STATE_WAIT_IPADDR :
        if (gotIpAddr && gotIp6Addr)
          newDevState(STATE_GOT_IPADDR);
        else
        {
          // Check the timeout.
          if (xTaskGetTickCount() >= xStartTime + xTicksTimeout)
          {
            printf("%% Timeout waiting for IP address\n");
            if (gotIpAddr || gotIp6Addr)
              newDevState(STATE_GOT_IPADDR);
            else
              newDevState(STATE_NEW);
          }
          else
          {
            vTaskDelay(NET_WAIT_MS / portTICK_PERIOD_MS);
          }
        }
        break;
      case STATE_GOT_IPADDR :
        newDevState(STATE_CONNECT);
        break;
      case STATE_CONNECT :
        // Initialize the Arduino library.
        libssh_begin();

        {
          int ex_rc = ex_main(state);
          printf("\n%% Execution completed: rc=%d\n", ex_rc);
        }
        newDevState(STATE_DONE);
      case STATE_DONE :
          enterDeepsleep();

          while (1)
          {
            vTaskDelay(60000 / portTICK_PERIOD_MS);
          }
          //todo proper task close
        // This would be the place to free net resources, if needed,
        return;
      default:
        break;
    }
  }

  printf("dont know how we reached here 2.\n");
  vTaskDelete(NULL);
}


void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    case ESP_SLEEP_WAKEUP_GPIO : 
    {
        Serial.println("Wakeup caused by GPIO program"); 
        uint64_t GPIO_reason = esp_sleep_get_gpio_wakeup_status();
        Serial.print("GPIO that triggered the wake up: GPIO ");
        Serial.println((log(GPIO_reason))/log(2), 0);
        break;
    }
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

doorCMD_t wakeSelector(){
  doorCMD_t res = CMD_UNKNOWN;
  u_int64_t wakeMask = esp_sleep_get_gpio_wakeup_status();

  if (wakeMask == 0b00100 ){
    res = CMD_OPEN;
  }
  else if (wakeMask == 0b10000){
    res = CMD_CLOSE;
  }

  return res;
}

#define Red_LED   3    // rgb led pins, active low
#define Green_LED 4
#define Blue_LED  5
#define LED      21     // onboard blue led


void setup()
{
  devState = STATE_NEW;

  // Use the expected blocking I/O behavior.
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  //Wake from deep sleep when these pins are high
  //gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT);
  gpio_set_direction(GPIO_NUM_2, GPIO_MODE_INPUT);

  gpio_set_direction(GPIO_NUM_3, GPIO_MODE_OUTPUT);

  digitalWrite(GPIO_NUM_3,0);

  Serial.begin(115200);
  uart_driver_install ((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
  esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

  doorCMD_t shackCMD = wakeSelector();

  switch (shackCMD){
    case CMD_UNKNOWN:   printf("UNKNOWN!\n");  enterDeepsleep();  break;
    case CMD_OPEN:      printf("OPEN!\n");   break;
    case CMD_CLOSE:      printf("CLOSE!\n");   break;
    default:  break;
  }

  //WiFi.setHostname("shackey");
  esp_netif_init();

  esp_event_loop_init(event_cb, NULL);
  // Stack size needs to be larger, so continue in a new task.
  xTaskCreatePinnedToCore(controlTask, "ctl", configSTACK, (void *) &shackCMD , (tskIDLE_PRIORITY + 3), NULL, portNUM_PROCESSORS - 1);

}


void loop()
{
  // Nothing to do here since controlTask has taken over.
  vTaskDelay(100 / portTICK_PERIOD_MS);
/*
  //somthing went wrong go to sleep
  if (esp_timer_get_time() > 40*1000*1000){//uptime > 20 sec 
    printf("took too long\n");
    enterDeepsleep();
  } 

  */
}