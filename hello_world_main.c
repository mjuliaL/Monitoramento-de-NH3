#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "nvs_flash.h"

#define SENSOR ADC1_CHANNEL_5  
#define BUZZER GPIO_NUM_13      

#define V_REF 3.3                  
#define ADC_MAX 4095               
#define valorcritico 25.0      

// Constantes de calibração
#define A 116.6020682
#define B -2.769034857

#define WIFI_SSID "note"
#define WIFI_PASS "note1234"
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

const char* api_key = "DJ0SHM6IF8DL5S3R";
const char* server = "http://api.thingspeak.com/update";

float calcular_ppm(float tensao) {
    if (tensao <= 0) {
        return 0;  
    }
    float razao = tensao / V_REF;  
    return A * pow(razao, B);      
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init() {
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
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    printf("Conectado ao Wi-Fi %s\n", WIFI_SSID);
}

void enviar_dados(int valor_bruto) {
    char url[256];
    snprintf(url, sizeof(url), "http://api.thingspeak.com/update?api_key=%s&field1=%d", api_key, valor_bruto);

    // Configuração do HTTP
    esp_http_client_config_t config = {
        .url = url,
    };

    // Inicializa HTTP
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        printf("Dados enviados com sucesso.\n");
    } else {
        printf("Erro ao enviar dados: %s\n", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void app_main(void) {
    // Inicializa o NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();

    // Configuração do ADC
    adc1_config_width(ADC_WIDTH_BIT_12);  // Resolução de 12 bits
    adc1_config_channel_atten(SENSOR, ADC_ATTEN_DB_12);  // Atenuação de 12 dB

    esp_rom_gpio_pad_select_gpio(BUZZER);
    gpio_set_direction(BUZZER, GPIO_MODE_OUTPUT);

    while (1) {
        
        int valorBruto = adc1_get_raw(SENSOR);
        printf("Valor Sensor: %d\n", valorBruto);

        enviar_dados(valorBruto);

        // Conversão de tensão
        float tensao = (valorBruto / (float)ADC_MAX) * V_REF;

        float ppm = calcular_ppm(tensao);
        printf("Tensão: %.2f V\n", tensao);

        if (valorBruto > valorcritico) {
            gpio_set_level(BUZZER, 1);  
            ESP_LOGI("Nível crítico atingido!");
        } else {
            gpio_set_level(BUZZER, 0);  
        }
        vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
}
