#ifndef GAP_H
#define GAP_H

#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "esp_log.h"
#include <string.h>
// Funktioner för att hantera GAP (Generisk Access Profile)
void ble_app_advertise(void);
void ble_app_on_sync(void);

#endif // GAP_H

