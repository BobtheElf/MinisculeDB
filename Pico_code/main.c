#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/rand.h"
#include <pico/time.h>

#define POTENTIOMETER_PIN 26  // GPIO pin connected to the potentiometer
#define BUTTON_PIN 15         // GPIO pin connected to the push button
#define LED_PIN 25
#define ARRAY_SIZE 100       // Number of readings to store
// Buffer size for the input string
#define BUFFER_SIZE 128
#define MS_BT_LOOP 100

// Struct to hold potentiometer and button status
struct DataPoint {
    uint16_t potentiometer_value;   // ADC value
    bool button_pressed;            // Button status
    bool led_on;                    // LED status
    long int ms_time;               // Timestamp in ms
};

/*
TODO:
1. Final topology out of options: Pi side
2. Epochs: determined experimentally, but pico needs a window to send and receive data: solved here and Pi side
3. Table: DataPoint Struct - solved here mostly but needs stuff on Pi side to make it better maybe - maybe determine max memory footprint here?
    3.1. Kick out data: store variable and index to kick out data with lowest deltas to replace with most current data (potentiometer value) - find out how much time's worth of data can be stored on the pico before the table fills up
4. SQL Logic: Unsolved - dissect message into SELECT, WHERE, and ORDER BY and then do table shenanigans
5. Parameters: Unsolved - Pi level vs Pico level query processing - epochs
*/

// This function's code is from: https://blog.smittytone.net/2021/10/31/how-to-send-data-to-a-raspberry-pi-pico-via-usb/
uint16_t get_block(uint8_t *buffer) {
  uint16_t buffer_index= 0;
  while (true) {
    int c = getchar_timeout_us(100);
    if (c != PICO_ERROR_TIMEOUT && buffer_index < BUFFER_SIZE) {
      buffer[buffer_index++] = (c & 0x1FF);
    } else {
      break;
    }
  }
  return buffer_index;
}

bool buf_comp(uint8_t *buf1, uint8_t *buf2, int len){
    for(int i = 0; i < len; i++){
        if(buf1[i] != buf2[i]){
            return true;
        }
    }
    return false;
}

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
    uint32_t pico_id = get_rand_32();

    //Keep track of the indices that need to be changed
    int loop_var = 0;
    
    // Create a buffer to store the input string
    char input_buffer[BUFFER_SIZE];
    int buffer_index = 0;
    char last_buffer[BUFFER_SIZE];
    int last_buffer_index = 0;

    //Initiate communication with the Pi flag
    bool speak = false;

    //Time to reference when measuring time since start
    uint64_t start = time_us_64();

    //Rank array for data points
    int rank[ARRAY_SIZE];

    //Loop forever
    while(true){

        //Keep LED on for the duration of one loop
        gpio_put(LED_PIN, true);

        //----------------------------------------------------------------------------------------------------
        //Read from serial
        int read_until = get_block(input_buffer); //returns final index of the buffer
        //Only echo if the input buffer and last buffer read are different
        bool is_different = false;
        if(read_until == last_buffer_index){
            is_different = buf_comp(last_buffer, input_buffer, read_until);
        }
        else{
            is_different = true;
        }
        if(is_different){
            //Make sure to clean out the buffer - get block can create garbage
            for(int i = read_until; i < BUFFER_SIZE; i++){
                input_buffer[i] = 0;
                last_buffer[i] = 0;
            }
            //Uncomment for echoing the output
            printf("Echo: %s\n", input_buffer);
            //Copy data from the input buffer to the last buffer to see if they are the same
            for(int i = 0; i < read_until; i++){
                last_buffer[i] = input_buffer[i];
            }
        }

        //Messages that could be sent
        char *helomsg = "HELO";
        char *timemsg = "TIME";
        char *query = "SELECT";

        //If the message is HELO send the Pico's id for communication - may be useful for broadcast information
        if((read_until == 4) && !(buf_comp(helomsg, input_buffer, read_until))){
            uint64_t end = time_us_64();
            uint64_t ms_used = end - start;
            printf("Pico ID: 0x%08X\nTime: %d", pico_id, ms_used);
        }
        //----------------------------------------------------------------------------------------------------

        //----------------------------------------------------------------------------------------------------
        //Collect data from the Pico
        data[loop_var].potentiometer_value = adc_read();            // Read potentiometer
        data[loop_var].button_pressed = gpio_get(BUTTON_PIN) == 0;  // Read button (active low)
        data[loop_var].led_on = true;
        
        //Get timestamp in ms - taken from https://stackoverflow.com/questions/3756323/how-to-get-the-current-time-in-milliseconds-from-c-in-linux
        data[loop_var].ms_time = (long int) get_absolute_time;

        //Code for displaying new data collected
        // printf("Reading %d: Potentiometer = %u, Button = %s\n", 
        //        loop_var, 
        //        data[loop_var].potentiometer_value, 
        //        data[loop_var].button_pressed ? "Pressed" : "Released");

        //Code for maybe deleting points to keep memory here

        //Go to the next loop value
        loop_var ++;
        if(loop_var >= ARRAY_SIZE){
            loop_var = 0;
        }
        //----------------------------------------------------------------------------------------------------

        //After all processes are complete, sleep the LED for the downtime
        gpio_put(LED_PIN, false);
        sleep_ms(MS_BT_LOOP);
    }

    return(0);
}