

/**
 * main.c
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "tiva_gps.h"
#include "tiva_log.h"
#include "tiva_sim.h"

#define SIM_PASSWORD        "123456789"
#define NUMBER_OF_COMMAND   1
#define MESSAGE_MAX_SIZE    30
#define MAP_STRING_SIZE     100

#define TAG                 "APP_MAIN"

static const char message_command_temp[NUMBER_OF_COMMAND][MESSAGE_MAX_SIZE] = {
                                               "#GPS%s.",       // #GPS{PASSWORD}.
};

static tiva_gps_handle_t gps_handle = NULL;
static tiva_sim_handle_t sim_handle = NULL;

#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}

#endif

//*****************************************************************************
void vApplicationStackOverflowHook(xTaskHandle *pxTask, char *pcTaskName)
{
    while (1);
}

static void app_main(void* pv)
{
    sim_message_info_t message_info;
    gps_t gps_data;
    char* map_string = (char*)malloc(MAP_STRING_SIZE);
    bool gps_available = false;
    while (1) {
        if (!gps_available) {
            if (tiva_gps_get_data(gps_handle, &gps_data)) {
                gps_available = true;
                tiva_sim_send_message(sim_handle, "GPS is available!", MANTAINER_PHONE);
            } else {
                vTaskDelay(1000 / portTICK_RATE_MS);
                continue;
            }
        }
        if (tiva_sim_read_message(sim_handle, &message_info)) {
            int i = 0;
            for (i = 0; i < NUMBER_OF_COMMAND; i++) {
                char check_command[MESSAGE_MAX_SIZE];
                int check_len = sprintf(check_command, message_command_temp[i], SIM_PASSWORD);
                check_command[check_len] = '\0';
                TIVA_LOGI(TAG, "Check command: %s", check_command);
                if (strstr(message_info.message, check_command) != NULL) {
                    if (tiva_gps_get_data(gps_handle, &gps_data)) {
                        int s_len = sprintf(map_string,"http://maps.google.com/maps?q=%f,%f&t=m&z=36",gps_data.latitude,gps_data.longitude);
                        map_string[s_len] = '\0';
                        TIVA_LOGI(TAG, "map string: %s", map_string);
                        tiva_sim_send_message(sim_handle, map_string, message_info.sender_phone);
                    } else {
                        tiva_sim_send_message(sim_handle, "GPS is not available!", message_info.sender_phone);
                    }
                    break;
                }
            }
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

int main(void)
{
    SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
    tiva_log_config_t log_config = {
                                    .uart_config = {
                                                    .hw_uart_num = HW_UART_NUM_0,
                                                    .baudrate = 115200,
                                                    .frame_config = HW_UART_FRAME_CONFIG_DEFAULT,
                                    },
                                    .enable_log_info = true,
                                    .enable_log_error = true,
                                    .enable_log_warning = false,
                                    .enable_log_debug = false,
    };
    tiva_log_init(log_config);

    tiva_gps_config_t gps_config = {
                                    .uart_config = {
                                                    .hw_uart_num = HW_UART_NUM_2,
                                                    .baudrate = 9600,
                                                    .frame_config = HW_UART_FRAME_CONFIG_DEFAULT,
                                    },
    };
    gps_handle = tiva_gps_init(gps_config);
    tiva_gps_run(gps_handle);
    TIVA_LOGI(TAG, "Init gps complete!");
    tiva_sim_config_t sim_config = {
                                    .uart_config = {
                                                    .hw_uart_num = HW_UART_NUM_1,
                                                    .baudrate = 115200,
                                                    .frame_config = HW_UART_FRAME_CONFIG_DEFAULT,
                                    },
    };
    sim_handle = tiva_sim_init(sim_config);
    tiva_sim_run(sim_handle);
    TIVA_LOGI(TAG, "Init Sim complete!");

    xTaskCreate(app_main, "app main", 512, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1) {

    }
	return 0;
}
