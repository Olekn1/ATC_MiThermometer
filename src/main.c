#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"

// Definicja pinu przycisku (TL_C2 w module BT3L to GPIO_PC2)
#define BUTTON_PIN      GPIO_PC2

// Struktura pakietu reklamowego BTHome V2 (Button Event)
typedef struct {
    u8 length;          // Długość danych
    u8 adv_type;        // Typ flagi (0x01)
    u8 flags;           // Wartość flagi (0x06 - General Discoverable)
    u8 bthome_len;      // Długość sekcji BTHome
    u8 bthome_type;     // Typ usługi (0x16 - Service Data 16-bit UUID)
    u16 bthome_uuid;    // UUID dla BTHome (0xFCD2 - format Little Endian)
    u8 device_info;     // BTHome device info (0x40 - BTHome V2, bez szyfrowania)
    u8 object_id;       // ID obiektu (0x3A - Button event)
    u8 event_value;     // Wartość zdarzenia (0x01 - Single Click)
} bthome_adv_t;

bthome_adv_t bthome_packet = {
    .length = 2,
    .adv_type = GAP_ADTYPE_FLAGS,
    .flags = 0x06,
    .bthome_len = 7,
    .bthome_type = GAP_ADTYPE_SERVICE_DATA_UUID_16BIT,
    .bthome_uuid = 0xD2FC, // 0xFCD2 po zamianie bajtów
    .device_info = 0x40,  // BTHome V2, unencrypted
    .object_id = 0x3A,    // Button Event
    .event_value = 0x01   // 0x01 = Single Press (Home Assistant to zrozumie)
};

int main(void) {
    // 1. Inicjalizacja systemu i zegarów (24 MHz)
    blc_pm_select_internal_32k_crystal();
    cpu_wakeup_init();
    clock_init(SYS_CLK_24M_Crystal);

    // 2. Konfiguracja GPIO dla przycisku (PC2)
    gpio_set_func(BUTTON_PIN, AS_GPIO);
    gpio_set_output_en(BUTTON_PIN, 0); // Wejście
    gpio_set_input_en(BUTTON_PIN, 1);  // Włączenie odczytu stanu
    gpio_setup_up_down_resistor(BUTTON_PIN, PM_PIN_PULLUP_10K); // Wewnętrzny Pull-up do 3V3

    // 3. Sprawdzenie, czy układ wstał ze snu przez wciśnięcie przycisku
    // Stan niski (0) na PC2 oznacza wciśnięcie (zwarcie do GND)
    if (gpio_read(BUTTON_PIN) == 0) {
        
        // Inicjalizacja kontrolera BLE i nadawania
        blc_ll_initAdvertising_module();
        bls_ll_setAdvData((u8*)&bthome_packet, sizeof(bthome_adv_t));
        
        // Ustawienie parametrów nadawania (szybkie nadawanie co 30ms)
        bls_ll_setAdvParam(30, 30, ADV_TYPE_CONNECTABLE_UNDIRECTED, OWN_ADDRESS_PUBLIC, 0, 0, BLT_ENABLE_ADV_ALL, ADV_FP_NONE);
        bls_ll_setAdvEnable(1);

        // Nadajemy pakiety przez ok. 1.5 sekundy, żeby Home Assistant na pewno je złapał
        for(volatile int i = 0; i < 50; i++) {
            sleep_us(30000); // 30 ms
        }
    }

    // 4. Konfiguracja ponownego budzenia układu stanem niskim na PC2
    cpu_set_gpio_wakeup(BUTTON_PIN, WAKEUP_LEVEL_LOW, 1);

    // 5. Głębokie uśpienie (Deep Sleep) do momentu kolejnego kliknięcia
    // W tym stanie pobór prądu spada do minimum, oszczędzając baterię
    cpu_sleep_wakeup(DEEP_SLEEP_MODE, PM_WAKEUP_PAD, 0);

    while (1) {
        // Pętla nigdy nie zostanie osiągnięta, bo układ zaśnie w kroku wyżej
    }
}
