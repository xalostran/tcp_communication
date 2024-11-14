#ifndef GATT_H
#define GATT_H

#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "esp_log.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <inttypes.h>
#include <string.h>

// Declare external service definitions
extern const struct ble_gatt_svc_def gatt_svcs[];
esp_err_t save_wifi_to_nvs(const char *ssid, const char *pass, const char *server_ip, uint16_t server_port);
esp_err_t load_wifi_from_nvs(char *ssid, size_t ssid_size, char *pass, size_t pass_size, char *server_ip, size_t ip_size, uint16_t *server_port);
void get_wifi_configuration(char *ssid, char *pass, char *server_ip, uint16_t *server_port);
void apply_new_wifi_configuration();
void ble_gatt_init();
int device_read(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

#endif // GATT_H