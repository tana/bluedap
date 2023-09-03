#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    for (;;) {
        printf("Hello world!\n");
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
