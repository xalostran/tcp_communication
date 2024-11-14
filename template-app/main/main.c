#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "gap.h"
#include "gatt.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "nimble/nimble_port_freertos.h"

#define SSID "YOUR SSID"
#define PASSWORD "YOUR PASSWORD"
#define IP_ADDRESS "YOUR IP"
#define PORT "YOUR PORT INTEGER"

const int CONNECTED_BIT = BIT0;
static EventGroupHandle_t wifi_event_group;
static const char *TAG = "tcp-client";
static const char *SERVER_TAG = "tcp-server";

#define TRIG_PIN GPIO_NUM_22 
#define ECHO_PIN GPIO_NUM_23 
#define LED_PIN GPIO_NUM_19
#define ALARM_PIN GPIO_NUM_18
#define SPEED_OF_SOUND 0.0343

void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

int64_t pulseIn(gpio_num_t pin, int level, int64_t timeout_us) {
    int64_t start_time = esp_timer_get_time();
    int64_t timeout_time = start_time + timeout_us;

    // Vänta på att signalen ska bli LOW eller HIGH
    while (gpio_get_level(pin) != level) {
        if (esp_timer_get_time() > timeout_time) {
            return 0; // Timeout, ingen puls upptäckt
            
        }
    }

    // Puls startade, mät starttiden
    int64_t pulse_start = esp_timer_get_time();

    // Vänta på att signalen ska ändra nivå igen
    while (gpio_get_level(pin) == level) {
        if (esp_timer_get_time() > timeout_time) {
            return 0; // Timeout, puls slutade aldrig
        }
    }

    // Puls slutade, mät slut-tiden och returnera pulslängden
    int64_t pulse_end = esp_timer_get_time();
    return pulse_end - pulse_start;
}


static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();

}

void print_ip_address(void)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        ESP_LOGI("IP_INFO", "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI("IP_INFO", "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI("IP_INFO", "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    }
    else
    {
        ESP_LOGE("IP_INFO", "Failed to get IP address");
    }
}

void init_ultrasonic_sensor()
{
    gpio_reset_pin(TRIG_PIN);
    gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(ECHO_PIN);
    gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);
}

float get_distance()
{
    // Ställ Trigger till låg för att säkerställa en ren puls
    gpio_set_level(TRIG_PIN, 0);
    vTaskDelay(2 / portTICK_PERIOD_MS);  // Vänta 2 µs för att stabilisera

    // Skicka en 10 µs hög signal på Trigger
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    // Vänta tills Echo blir hög (början av retursignal)
    int64_t timeout = 60000; // Timeout på 40 ms

    // Beräkna pulsens längd
    float duration = pulseIn(ECHO_PIN, 1, timeout);
    // Beräkna avstånd (cm)
    float distance = (duration * SPEED_OF_SOUND) / 2.0;

    ESP_LOGI(TAG, "Distance measured: %.2f cm", distance);
    
    return distance;
}


void init_led_and_alarm()
{
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(ALARM_PIN);
    gpio_set_direction(ALARM_PIN, GPIO_MODE_OUTPUT);
}

/*void get_server_config(char *ip, size_t ip_size, uint16_t *port) {
    load_wifi_from_nvs(NULL, 0, NULL, 0, ip, ip_size, port);
}*/

void tcp_client(void *pvParameters)
{
    char buffer[128];
    
    char ssid[32], password[64], ip[16];
    uint16_t port;

    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;


    while (1)
    {
        struct sockaddr_in destination_addr;
        destination_addr.sin_addr.s_addr = inet_addr(ip);
        destination_addr.sin_family = AF_INET;
        destination_addr.sin_port = htons(port);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sockfd = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sockfd < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Socket successfully created.");

        // Hämta dynamisk IP och port från NVS
        get_wifi_configuration(ssid, password, ip, &port);


        int err_t = connect(sockfd, (struct sockaddr *)&destination_addr, sizeof(destination_addr));
        if (err_t != 0)
        {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d. Current ip:port: %s:%d", errno, ip, port);
            close(sockfd);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Socket connected successfully to %s:%d", ip, port);


        float distance = get_distance();
        ESP_LOGI(TAG, "Sending distance to server: %.2f cm", distance);
        snprintf(buffer, sizeof(buffer), "Distance: %.2f cm.", distance);

        int err = send(sockfd, buffer, strlen(buffer), 0);

        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occured when sending data : errno %d", errno);
            close(sockfd);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Data sent to server.");
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        int len = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        ESP_LOGI(TAG, "Measured distance: %.2f cm", get_distance());
        if (len > 0)
        {
            buffer[len] = 0;

            if (strstr(buffer, "TURN_ON_LED") != NULL)
            {
                gpio_set_level(LED_PIN, 1);
                ESP_LOGI(SERVER_TAG, "LED turned on");
            }
            else if (strstr(buffer, "TURN_OFF_LED") != NULL)
            {
                gpio_set_level(LED_PIN, 0);
                ESP_LOGI(SERVER_TAG, "LED turned off");
            }

            if (strstr(buffer, "TURN_ON_ALARM") != NULL)
            {
                gpio_set_level(ALARM_PIN, 1);
                ESP_LOGI(SERVER_TAG, "Alarm turned on");
            }
            else if (strstr(buffer, "TURN_OFF_ALARM") != NULL)
            {
                gpio_set_level(ALARM_PIN, 0);
                ESP_LOGI(SERVER_TAG, "Alarm turned off");
            }
        }
        close(sockfd);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void ble_gatt_init();

void ble_app_on_sync();

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    char ssid[32], password[64], ip[16];
    uint16_t port;
    
    get_wifi_configuration(ssid, password, ip, &port);

    // Initiera alla periferienheter
    init_led_and_alarm();
    init_ultrasonic_sensor();

    // Starta BLE och ange dess konfigurationer
    wifi_init();  // Initiera WiFi stacken men anslut inte ännu
    nimble_port_init();

    ble_svc_gap_device_name_set("BLE-");
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatt_init();

    // Callback för BLE sync
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    nimble_port_freertos_init(host_task);  // Start BLE Task

    // Vänta på WiFi-anslutning efter att ha tagit emot BLE-data
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    // Starta TCP-klient om anslutningen till WiFi är klar
    xTaskCreate(tcp_client, "tcp_client", 4096, NULL, 5, NULL);
}