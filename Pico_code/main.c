#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define POTENTIOMETER_PIN 26  // GPIO pin connected to the potentiometer
#define LED_PIN 25
#define ARRAY_SIZE 100       // Number of readings to store

int main(){

    uint16_t readings[ARRAY_SIZE];

    // Initialize the ADC
    adc_init();
    
    // Set up the ADC pin
    adc_gpio_init(POTENTIOMETER_PIN); // GPIO 26 is ADC0
    adc_select_input(0);             // Use ADC input 0 (GPIO 26)

    //Initialize LED Pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    //Initialize chosen serial port
    stdio_init_all();

    //Keep track of the indices that need to be changed
    int loop_var = 0;

    //Loop forever
    while(true){

        //Blink LED
        printf("Blinking\r\n");
        gpio_put(LED_PIN, true);
        sleep_ms(1500);
        gpio_put(LED_PIN, false);
        readings[loop_var] = adc_read(); // Read the potentiometer value
        printf("Reading %d: %u\n", loop_var, readings[loop_var]); // Print the readings to the console
        loop_var ++;
        if(loop_var > ARRAY_SIZE){
            loop_var = 0;
        }
        sleep_ms(500);
    }

    return(0);
}