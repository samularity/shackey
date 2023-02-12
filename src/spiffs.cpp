#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

static const char *TAG = "SPIFFS";

esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 2, //max. simultaneous open files 
    .format_if_mount_failed = true
};

esp_err_t initSPIFFS(){

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
    }

    return ret;
}

void printSPIFFSinfo(){

    DIR* dir = NULL;
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(conf.partition_label, &total, &used);

    if (ret == ESP_OK) {
        printf("Partition size: total: %d, used: %d\n", total, used );
    } else {
        printf("error reading spiffs info\n");
        return;
    }


    dir = opendir("/spiffs");
    if (dir == NULL) {
        return;
    }
    while (true) {
        struct dirent* de = readdir(dir);
        if (!de) {
            break;
        }
        printf("Found file: %s\n", de->d_name);
    }

    return;
}

void deinitSPIFFS(){
    // All done, unmount partition and disable SPIFFS
    esp_vfs_spiffs_unregister(conf.partition_label);
    ESP_LOGI(TAG, "SPIFFS unmounted");
    return;
}