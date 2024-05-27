/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/ledc.h"


#define ldrPin          ADC_CHANNEL_6
#define ledPin          GPIO_NUM_2
#define RXD_PIN         GPIO_NUM_3
#define TXD_PIN         GPIO_NUM_1
#define UART            UART_NUM_0
#define set_duty(duty)  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty)
#define upt_duty        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0)


static int adc_raw;
const static char *TAG = "DEVTITANS";
static const int RX_BUF_SIZE = 1024;

int ldrMax = 4095;
uint8_t ledValue = 10;
int ldrValue = 49;

adc_oneshot_unit_handle_t adc1_handle;

int map(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

char **check_set_led(char *command){

    uint8_t total_strings = 2;
    uint8_t string_size = 10;
    uint8_t i;
    uint8_t j = 0;

    char **array = (char **) malloc(total_strings * sizeof(char));

    for (int k = 0; k < total_strings; k++) {
        array[k] = (char *) malloc(string_size + 1);
    }

    for(i = 0; i < strlen(command); i++){

        if(command[i] == ' '){
            array[0][i] = '\0';
            i++;
            break;
        }
        array[0][i] = command[i];
    }

    for(; i< strlen(command); i++){
        array[1][j] = command[i];
        j++;
    }

    array[1][j] = '\0';

    return array;
}

void ledUpdate() {
    // Valor deve convertar o valor recebido pelo comando SET_LED para 0 e 255
    // Normalize o valor do LED antes de enviar para a porta correspondente
    set_duty(ledValue);
    upt_duty;
}

// Função para ler o valor do LDR
int ldrGetValue() {
    // Leia o sensor LDR e retorne o valor normalizado entre 0 e 100
    // faça testes para encontrar o valor maximo do ldr (exemplo: aponte a lanterna do celular para o sensor)       
    // Atribua o valor para a variável ldrMax e utilize esse valor para a normalização

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ldrPin, &adc_raw));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ldrPin, adc_raw);

    int mapped_value = map(adc_raw, 0, ldrMax, 0, 100);
    ESP_LOGI(TAG, "Mapped value: %d", mapped_value);

    return mapped_value;
}

void processCommand(char *command) {
    if(strcmp("GET_LED", command) == 0){
        char str[20];
        sprintf(str, "RES GET_LED %d", ledValue);
        uart_write_bytes(UART, str, strlen(str));
    }
    else if(strcmp("GET_LDR", command) == 0){
        char str[20];
        sprintf(str, "RES GET_LDR %d", ldrValue);
        uart_write_bytes(UART, str, strlen(str));
    }
    else{
        char **set_led = check_set_led(command);

        if((strcmp("SET_LED", set_led[0]) == 0) && set_led[1]){
            int value = atoi(set_led[1]);

            if(value >= 0 && value <= 100){
                char *str = "RES SET_LED 1";
                uart_write_bytes(UART, str, strlen(str));
                ledValue = map(value, 0, 100, 0, 255);
                ESP_LOGI(TAG, "mapped: %d", ledValue);
                ledUpdate();
            }
            else{
                char *str = "RES SET_LED -1";
                uart_write_bytes(UART, str, strlen(str));
            }
        }
        else{
            char *str = "ERR Unknown command";
            uart_write_bytes(UART, str, strlen(str));
        }
        
        free(set_led);
    } 
}

void init_uart() 
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // We won't use a buffer for sending data.
    uart_driver_install(UART, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART, &uart_config);
    uart_set_pin(UART, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void pwm_init()
{
    ledc_timer_config_t pwm_timer = {
        .speed_mode         = LEDC_LOW_SPEED_MODE,
        .timer_num          = LEDC_TIMER_0,
        .duty_resolution    = LEDC_TIMER_8_BIT,
        .freq_hz            = 2000,
        .clk_cfg            = LEDC_AUTO_CLK
    };

    ledc_timer_config(&pwm_timer);

    ledc_channel_config_t pwm_ch = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = ledPin,
        .duty       = 0,
        .hpoint     = 0
    };

    ledc_channel_config(&pwm_ch);
}


static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    char* data = (char*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART, data, RX_BUF_SIZE, 500 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            processCommand(data);
            
        }
    }
    free(data);
}



void app_main(void)
{   
    //-------------ADC1 Init---------------//
    
    adc_oneshot_unit_init_cfg_t init_config1 = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = { .bitwidth = ADC_BITWIDTH_DEFAULT, .atten = ADC_ATTEN_DB_12 };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ldrPin, &config));

    init_uart();
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);

    pwm_init();
    set_duty(ledValue);
    upt_duty;

    while (1) {

        

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}