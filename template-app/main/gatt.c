#include "gatt.h"
#include <string.h>

#define NVS_NAMESPACE "storage"

static const char *GATT_TAG = "BLE-GATT";

static char wifi_ssid[32] = {0};
static char wifi_pass[64] = {0};
static char server_ip[16] = {0};
static uint16_t server_port = 0;

esp_err_t save_wifi_to_nvs(const char *ssid, const char *pass, const char *server_ip, uint16_t server_port) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to save SSID: %s", esp_err_to_name(err));
    }
    err = nvs_set_str(nvs_handle, "pass", pass);
    if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to save password: %s", esp_err_to_name(err));
    }
    err = nvs_set_str(nvs_handle, "server_ip", server_ip);
    if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to save server IP: %s", esp_err_to_name(err));
    }
    err = nvs_set_u16(nvs_handle, "server_port", server_port);
    if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to save server port: %s", esp_err_to_name(err));
    }
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to commit changes: %s", esp_err_to_name(err));
    }
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

esp_err_t load_wifi_from_nvs(char *ssid, size_t ssid_size, char *pass, size_t pass_size, char *server_ip, size_t ip_size, uint16_t *server_port) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_get_str(nvs_handle, "ssid", NULL, &ssid_size);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_size);
    } else if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to load SSID: %s", esp_err_to_name(err));
    }
    err = nvs_get_str(nvs_handle, "pass", NULL, &pass_size);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, "pass", pass, &pass_size);
    } else if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to load password: %s", esp_err_to_name(err));
    }
    err = nvs_get_str(nvs_handle, "server_ip", NULL, &ip_size);
    if (err == ESP_OK) {
        err = nvs_get_str(nvs_handle, "server_ip", server_ip, &ip_size);
    } else if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to load server IP: %s", esp_err_to_name(err));
    }
    err = nvs_get_u16(nvs_handle, "server_port", server_port);
    if (err != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to load server port: %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
    return ESP_OK;
}

void get_wifi_configuration(char *ssid, char *pass, char *server_ip, uint16_t *server_port) {

    size_t ssid_size = 32;
    size_t pass_size = 64;
    size_t ip_size = 16;

    esp_err_t err = load_wifi_from_nvs(ssid, ssid_size, pass, pass_size, server_ip, ip_size, server_port);
    if (err != ESP_OK){
        ESP_LOGE(GATT_TAG, "No valid config found: %s", esp_err_to_name(err));
    }else{
        ESP_LOGI(GATT_TAG, "Loaded from NVS: SSID: %s, IP: %s, Port: %" PRIu16, ssid, server_ip, *server_port);
    }
}

// Write handler (implement your logic here)
int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    char *data = (char *)ctxt->om->om_data;
    ESP_LOGI(GATT_TAG, "Received data: %.*s", ctxt->om->om_len, ctxt->om->om_data);

    char *token = strtok(data, ";");
    while (token != NULL) {
        if (strncmp(token, "SSID=", 5) == 0) {
            strncpy(wifi_ssid, token + 5, sizeof(wifi_ssid) - 1);
            wifi_ssid[sizeof(wifi_ssid) - 1] = '\0';
            ESP_LOGI(GATT_TAG, "Received SSID: %s", wifi_ssid);
        } else if (strncmp(token, "PASS=", 5) == 0) {
            strncpy(wifi_pass, token + 5, sizeof(wifi_pass) - 1);
            wifi_pass[sizeof(wifi_pass) - 1] = '\0';
            ESP_LOGI(GATT_TAG, "Received Password: %s", wifi_pass);
        } else if (strncmp(token, "SERVER_IP=", 10) == 0) {
            strncpy(server_ip, token + 10, sizeof(server_ip) - 1);
            server_ip[sizeof(server_ip) - 1] = '\0';
            ESP_LOGI(GATT_TAG, "Received Server IP: %s", server_ip);
        } else if (strncmp(token, "SERVER_PORT=", 12) == 0) {
            server_port = atoi(token + 12);
            ESP_LOGI(GATT_TAG, "Received Server Port: %d", server_port);
        } else {
            ESP_LOGI(GATT_TAG, "Unknown command: %s", token);
        }
        token = strtok(NULL, ";");
    }
    // Kontrollera om alla parametrar har mottagits och applicera konfigurationen
    if (strlen(wifi_ssid) > 0 && strlen(wifi_pass) > 0 && strlen(server_ip) > 0 && server_port > 0) {
        ESP_LOGI(GATT_TAG, "Applying new WiFi and Server configuration");
        save_wifi_to_nvs(wifi_ssid, wifi_pass, server_ip, server_port);
        apply_new_wifi_configuration();  // Dynamisk WiFi-omstart
    }
    return 0;
}

// Read data from ESP32 defined as server
int device_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    char ssid[32] = {0};
    char pass[64] = {0};
    char server_ip[16] = {0};
    uint16_t server_port = 0;
    get_wifi_configuration(ssid, pass, server_ip, &server_port);

    char response[256];
    snprintf(response, sizeof(response), "SSID=%s;PASS=%s;SERVER_IP=%s;SERVER_PORT=%d",
             ssid, pass, server_ip, server_port);

    os_mbuf_append(ctxt->om, response, strlen(response));
    return 0;
}

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
const struct ble_gatt_svc_def gatt_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x180A),                 // Define UUID for device type
     .characteristics = (struct ble_gatt_chr_def[]){
         {
            .uuid = BLE_UUID16_DECLARE(0x2A56),           // Define UUID for reading
            .flags = BLE_GATT_CHR_F_READ,
            .access_cb = device_read
          },
         {
            .uuid = BLE_UUID16_DECLARE(0x2A57),           // Define UUID for writing
            .flags = BLE_GATT_CHR_F_WRITE,
            .access_cb = device_write
          },
          {0}
        }
    },
    {0}
};


void apply_new_wifi_configuration() {
    esp_err_t ret;

    // Stopping and restarting WiFi to apply the new configuration
    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to stop WiFi: %s", esp_err_to_name(ret));
        return;
    }

    esp_wifi_stop();
    esp_wifi_start();

    wifi_config_t configure_wifi = {
        .sta = {
            .ssid = {0},
            .password = {0},
        },
    };

    strncpy((char *)configure_wifi.sta.ssid, wifi_ssid, sizeof(configure_wifi.sta.ssid) - 1);
    strncpy((char *)configure_wifi.sta.password, wifi_pass, sizeof(configure_wifi.sta.password) - 1);

    ESP_LOGI(GATT_TAG, "Setting new WiFi configuration: SSID=%s, PASSWORD=%s", configure_wifi.sta.ssid, configure_wifi.sta.password);

    ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &configure_wifi);
    if (ret != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to configure WiFi: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(GATT_TAG, "Failed to connect to WiFi: %s", esp_err_to_name(ret));
        return;
    }
}

void ble_gatt_init() {
    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(GATT_TAG, "Failed to count GATT services: %d", rc);
        return;
    }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(GATT_TAG, "Failed to add GATT services: %d", rc);
        return;
    }
}