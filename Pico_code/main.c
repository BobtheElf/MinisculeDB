#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/rand.h"

#define POTENTIOMETER_PIN 26  // GPIO pin connected to the potentiometer
#define BUTTON_PIN 15         // GPIO pin connected to the push button
#define LED_PIN 25
#define ARRAY_SIZE 100       // Number of readings to store

// Struct to hold potentiometer and button status
struct DataPoint {
    uint16_t potentiometer_value;  // ADC value
    bool button_pressed;           // Button status
    bool led_on;                    // LED status
};

int main(){
    // Array to store the data
    struct DataPoint data[ARRAY_SIZE];

    //Initialize chosen serial port
    stdio_init_all();

    // Initialize the ADC
    adc_init();
    
    // Set up the ADC pin
    adc_gpio_init(POTENTIOMETER_PIN); // GPIO 26 is ADC0
    adc_select_input(0);             // Use ADC input 0 (GPIO 26)

    // Initialize the button pin
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);        // Enable pull-up resistor

    //Initialize LED Pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    //Get a random id value
    uint32_t picoID = get_rand_32();

    //Keep track of the indices that need to be changed
    int loop_var = 0;

    //Loop forever
    while(true){

        //Blink LED
        printf("Blinking\r\n");
        gpio_put(LED_PIN, true);
        sleep_ms(1500);
        gpio_put(LED_PIN, false);
        
        //Collect data from the Pico
        data[loop_var].potentiometer_value = adc_read();            // Read potentiometer
        data[loop_var].button_pressed = gpio_get(BUTTON_PIN) == 0;  // Read button (active low)
        data[loop_var].led_on = true;

        //Go to the next loop value
        loop_var ++;
        if(loop_var > ARRAY_SIZE){
            loop_var = 0;
        }
        sleep_ms(500);
    }

    return(0);
}