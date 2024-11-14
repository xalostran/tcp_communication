#include "gap.h"

static const char *TAG = "BLE-GAP";

// Global variabel för BLE-adresstypen som används under annonsering och anslutning
static uint8_t ble_addr_type;

// Denna funktion är en BLE GAP-eventhanterare som hanterar händelser relaterade till BLE-anslutningar
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        //Händelse när en BLE-enhet försöker ansluta
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
            // Om anslutningen misslyckas, starta advertising på nytt
            if (event->connect.status != 0) {
                ble_app_advertise();
            }
            break;
            // Händelse när BLE-enheten kopplas bort
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE GAP EVENT DISCONNECTED");
            // Starta om advertising när frånkoppling sker
            ble_app_advertise();
            break;
            // Händelse när advertising är avslutad
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "BLE GAP EVENT ADV COMPLETE");
            // när advertising är avslutad, starta en ny advertising
            ble_app_advertise();
            break;
        default:
            break;
    }
    return 0;
}

//funktionen för att starta BLE advertising
void ble_app_advertise(void) {
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    // Hämtar enhetens namn och ställer in det som ett komplett namn i advertising fields
    device_name = ble_svc_gap_device_name(); // Hämta enhetsnamnet
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1; // Indikerar att namnet är komplett
    // Ställer in advertising fields
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;// Ange att anslutning är möjlig
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // Ange att enheten ska vara allmänt synlig
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// Anropas när BLE-stacken är synkad
void ble_app_on_sync(void) {
     // Hämtar automatisk BLE-adresstyp och startar annonsering
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_app_advertise();
}


