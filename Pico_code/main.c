#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/rand.h"
#include <pico/time.h>
#include <stdlib.h>
#include <ctype.h>
// #include <string.h>

#define POTENTIOMETER_PIN 26  // GPIO pin connected to the potentiometer
#define BUTTON_PIN 15         // GPIO pin connected to the push button
#define LED_PIN 25
#define ARRAY_SIZE 500       // Number of readings to store
// Buffer size for the input string
#define BUFFER_SIZE 128
#define MS_BT_LOOP 100

// Struct to hold potentiometer and button status
struct DataPoint {
    long int ms_time;               // Timestamp in ms
    uint16_t potentiometer_value;   // ADC value
    bool button_pressed;            // Button status
    bool led_on;                    // LED status
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

//Comparison functions for ORDER BY
int compareDPTime(const void* a, const void* b){
    return ((struct DataPoint*) a) -> ms_time - ((struct DataPoint*) b) -> ms_time;
}
int compareDPPot(const void* a, const void* b){
    return ((struct DataPoint*) a) -> potentiometer_value - ((struct DataPoint*) b) -> potentiometer_value;
}
int compareDPBut(const void* a, const void* b){
    return ((struct DataPoint*) a) -> button_pressed - ((struct DataPoint*) b) -> button_pressed;
}
int compareDPLed(const void* a, const void* b){
    return ((struct DataPoint*) a) -> led_on - ((struct DataPoint*) b) -> led_on;
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
    //Keep track of the size of the array
    int num_samples = 0;
    //Follows num_samples until it is too large, and then just follows ARRAY_SIZE
    int arr_len = 0;
    
    // Create a buffer to store the input string
    char input_buffer[BUFFER_SIZE];
    int buffer_index = 0;
    char last_buffer[BUFFER_SIZE];
    int last_buffer_index = 0;

    //Initiate communication with the Pi flag
    bool speak = false;

    //Time to reference when measuring time since start
    uint64_t start = time_us_64();
    uint64_t end = time_us_64();
    uint64_t ms_used = time_us_64();
    uint64_t loop_start = time_us_64();
    uint64_t loop_end = time_us_64();
    uint64_t loop_time = time_us_64();

    //For testing
    double cur_variance_sum = 0;

    //Query attributes
    bool select = false;
    int select_subj = 0;
    bool where = false;
    int where_var = 0;
    char where_op = 0; //0 - x, 1 - <, 2 - >, 3 - =, 4 - <=, 5 - >=, 6 - <>
    int where_val = 0;
    bool orderby = false;
    int orderby_pred = 0;
    //Grammar I guess: SELECT [var](,[var])?(,[var])?(,[var])?( WHERE [var][op][value])( ORDER BY [var](,[var])?(,[var])?(,[var])?)?
    //Valid var names: "time" - ms_time; "potv" - potentiometer_value; "butp" - button_pressed; "ledo" - led_on
    //Var encodings - time = 1000; potv = 100; butp = 10; ledo = 1

    //Loop forever
    while(true){

        //----------------------------------------------------------------------------------------------------
        //Housekeeping for the start of every loop
        loop_start = time_us_64();
        //Keep LED on for the duration of one loop - duty cycle is a good indication of how hard it's working
        gpio_put(LED_PIN, true);
        //Recalculate at the start of every loop
        arr_len = num_samples > ARRAY_SIZE ? ARRAY_SIZE : num_samples;
        //----------------------------------------------------------------------------------------------------

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
            // printf("Echo: %s\n", input_buffer);
            //Copy data from the input buffer to the last buffer so that they are the same
            for(int i = 0; i < read_until; i++){
                last_buffer[i] = input_buffer[i];
            }
        }
        //----------------------------------------------------------------------------------------------------

        //----------------------------------------------------------------------------------------------------
        //Interpret messages that could be sent
        char *helomsg = "HELO";
        char *timemsg = "TIME";
        char *dumpmsg = "DUMP";
        char *querymsg = "SELECT";

        //If the message is HELO send the Pico's id for communication - may be useful for broadcast information
        if((read_until == 4) && !(buf_comp(helomsg, input_buffer, read_until))){
            end = time_us_64();
            ms_used = end - start;
            printf("Pico ID: 0x%08X\nTime: %lu\nEHLO\n", pico_id, ms_used);
        }
        //If the message is TIME send the current timestamp - may be useful for synchronizing the epochs
        if((read_until == 4) && !(buf_comp(timemsg, input_buffer, read_until))){
            printf("Loop Time: %lu %lu %lu\n", loop_time, loop_end, loop_start);
        }
        //If the message is DUMP send the data - may be useful for debugging and such
        if((read_until == 4) && !(buf_comp(dumpmsg, input_buffer, read_until))){
            end = time_us_64();
            ms_used = end - start;
            // printf("Current variance sum: %f\n", cur_variance_sum);
            printf("Pico ID: 0x%08X\nTime: %lu\nDumping %d lines\n", pico_id, ms_used, arr_len);
            for(int i = 0; i < arr_len; i++){
                printf("Index: %d\tTimestamp: %lu\tPotentiometer %d\tButton: %d\tLED: %d\n",
                i, data[i].ms_time, data[i].potentiometer_value, data[i].button_pressed, data[i].led_on);
            }
        }
        //Else determine if it is a query - only so much checking I'm going to do here
        if((read_until > 6) && !(buf_comp(querymsg, input_buffer, 6))){
            int cur_idx = 7;
            select = false;
            select_subj = 0;
            where = false;
            where_var = 0;
            where_op = 0; //0 - x, 1 - <, 2 - >, 3 - =, 4 - <=, 5 - >=, 6 - !=
            where_val = 0;
            orderby = false;
            orderby_pred = 0;
            bool wherec = false;
            while(cur_idx < read_until){
                // printf("Iteration through the cur_idx %d\n", cur_idx);
                if(!select){
                    if((input_buffer[cur_idx] == ' ') && (input_buffer[cur_idx + 1] == 'W')){
                        cur_idx += 7;
                        select = true;
                        where = true;
                    }
                    else if((input_buffer[cur_idx] == ' ') && (input_buffer[cur_idx + 1] == 'O')){
                        cur_idx += 10;
                        select = true;
                        wherec = true;
                        orderby = true;
                    }
                    else if(input_buffer[cur_idx] == 't'){
                        select_subj += 1000;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'p'){
                        select_subj += 100;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'b'){
                        select_subj += 10;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'l'){
                        select_subj += 1;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == '*'){
                        select_subj = 1111;
                        cur_idx ++;
                    }
                    else{
                        cur_idx ++; //I think this technically means that anything could be a valid query
                    }
                }
                else if(!wherec){
                    //End of where clause
                    if((input_buffer[cur_idx] == ' ') && (input_buffer[cur_idx + 1] == 'O')){
                        cur_idx += 10;
                        orderby = true;
                        wherec = true;
                    }
                    //Where clause operand
                    else if(input_buffer[cur_idx] == 't'){
                        where_var += 1000;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'p'){
                        where_var += 100;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'b'){
                        where_var += 10;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'l'){
                        where_var += 1;
                        cur_idx += 4;
                    }
                    //Where clause operator
                    else if(input_buffer[cur_idx] == '!'){
                        where_op = 3;
                        cur_idx ++;
                    }
                    else if(input_buffer[cur_idx] == '<'){
                        where_op = 1;
                        cur_idx ++;
                    }
                    else if(input_buffer[cur_idx] == '>'){
                        where_op = 2;
                        cur_idx ++;
                    }
                    else if(input_buffer[cur_idx] == '='){
                        where_op += 3;
                        cur_idx ++;
                    }
                    //Where clause value
                    else if(isdigit(input_buffer[cur_idx])){
                        where_val *= 10;
                        where_val += input_buffer[cur_idx] - 48; //Just subtract the ascii value of the character
                        cur_idx ++;
                    }
                    else{
                        cur_idx ++;
                    }
                }
                else{
                    if(input_buffer[cur_idx] == 't'){
                        orderby_pred += 1000;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'p'){
                        orderby_pred += 100;
                        orderby = true;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'b'){
                        orderby_pred += 10;
                        orderby = true;
                        cur_idx += 4;
                    }
                    else if(input_buffer[cur_idx] == 'l'){
                        orderby_pred += 1;
                        orderby = true;
                        cur_idx += 4;
                    }
                    else{
                        cur_idx ++; //I think this technically means that anything could be a valid query
                    }
                }
            }
            printf("Result: %d, %d, %d, %d, %d, %d, %d, %d\n", select, select_subj, where, where_var, where_op, where_val, orderby, orderby_pred);
        }
        //----------------------------------------------------------------------------------------------------

        //----------------------------------------------------------------------------------------------------
        //Parse SQL logic
        if(select){
            printf("Parsing\n");
            struct DataPoint pool[ARRAY_SIZE]; //Super lazy and space-consuming, but that is a later fix
            int count = 0;
            //Test for funzies
            pool[0] = data[0];
            printf("%lu:%lu; %d:%d; %d:%d; %d:%d\n", pool[0].ms_time, data[0].ms_time, pool[0].potentiometer_value, 
            data[0].potentiometer_value, pool[0].button_pressed, data[0].button_pressed, pool[0].led_on, data[0].led_on);
            if(where){
                printf("Interpreting WHERE clause\n");
                //Interpret the where clause - whereval is given, operator needs to be translated
                if(where_var == 1000){
                    if(where_op == 1){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].ms_time < where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 2){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].ms_time > where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 3){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].ms_time == where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 4){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].ms_time <= where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 5){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].ms_time >= where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 6){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].ms_time != where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                }
                else if(where_var == 100){
                    if(where_op == 1){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].potentiometer_value < where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 2){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].potentiometer_value > where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 3){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].potentiometer_value == where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 4){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].potentiometer_value <= where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 5){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].potentiometer_value >= where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 6){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].potentiometer_value != where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                }
                else if(where_var == 10){
                    if(where_op == 1){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].button_pressed < where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 2){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].button_pressed > where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 3){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].button_pressed == where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 4){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].button_pressed <= where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 5){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].button_pressed >= where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 6){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].button_pressed != where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                }
                else if(where_var == 1){
                    if(where_op == 1){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].led_on < where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 2){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].led_on > where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 3){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].led_on == where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 4){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].led_on <= where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 5){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].led_on >= where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                    if(where_op == 6){
                        for(int i = 0; i < arr_len; i++){
                            if(data[i].led_on != where_val){
                                pool[i] = data[i];
                                count ++;
                            }
                        }
                    }
                }
                
            }
            else{
                printf("Skipping WHERE clause\n");
                count = arr_len;
                for(int i = 0; i < arr_len; i++){
                    pool[i] = data[i];
                }
            }
            if(orderby){
                printf("Interpreting ORDER BY\n");
                //I am not sure how to do mappings, so I guess I am just going to have to repeat myself a bunch of times again
                if((orderby_pred % 10) == 1){
                    qsort(pool, count, sizeof(struct DataPoint), &compareDPLed);
                }
                else if((orderby_pred % 100) >= 10){
                    qsort(pool, count, sizeof(struct DataPoint), &compareDPBut);
                }
                else if((orderby_pred % 1000) >= 100){
                    qsort(pool, count, sizeof(struct DataPoint), &compareDPPot);
                }
                else if(orderby_pred >= 1000){
                    qsort(pool, count, sizeof(struct DataPoint), &compareDPTime);
                }
            }
            else{
                printf("Skipping ORDER BY\n");
            }
            //Projection last - actually printed
            printf("Projecting %d over array size %d\n", select_subj, count);
            //Printing headers
            if(select_subj >= 1000){
                printf("time");
                if((select_subj - 1000) > 0){
                    printf(", ");
                }
            }
            if((select_subj % 1000) >= 100){
                printf("potv");
                if(((select_subj % 1000) - 100) > 0){
                    printf(", ");
                }
            }
            if((select_subj % 100) >= 10){
                printf("butp");
                if(((select_subj % 100) - 10) > 0){
                    printf(", ");
                }
            }
            if((select_subj % 10) == 1){
                printf("ledo");
            }
            printf("\n");
            for(int i = 0; i < count; i ++){
                struct DataPoint point = pool[i];
                if(select_subj >= 1000){
                    printf("%lu", point.ms_time);
                    if((select_subj - 1000) > 0){
                        printf(", ");
                    }
                }
                if((select_subj % 1000) >= 100){
                    printf("%d", point.potentiometer_value);
                    if(((select_subj % 1000) - 100) > 0){
                        printf(", ");
                    }
                }
                if((select_subj % 100) >= 10){
                    printf("%d", point.button_pressed);
                    if(((select_subj % 100) - 10) > 0){
                        printf(", ");
                    }
                }
                if((select_subj % 10) == 1){
                    printf("%d", point.led_on);
                }
                printf("\n");
            }
        }
        //After all the logic is done, reset the values so that the logic is only parsed once per new SQL statement
        select = false;
        select_subj = 0;
        where = false;
        where_var = 0;
        where_op = 0; //0 - x, 1 - <, 2 - >, 3 - =, 4 - <=, 5 - >=, 6 - <>
        where_val = 0;
        orderby = false;
        orderby_pred = 0;
        //----------------------------------------------------------------------------------------------------

        //----------------------------------------------------------------------------------------------------
        //Collect data from the Pico
        data[loop_var].potentiometer_value = adc_read();            // Read potentiometer
        data[loop_var].button_pressed = gpio_get(BUTTON_PIN) == 0;  // Read button (active low)
        data[loop_var].led_on = true;                               // Set the value of the led to either boolean variable
        
        end = time_us_64();
        data[loop_var].ms_time = end - start;                       // Read the timestamp in ms since the start of the program

        //Code for displaying new data collected
        // printf("Reading %d: Potentiometer = %u, Button = %s\n", 
        //        loop_var, 
        //        data[loop_var].potentiometer_value, 
        //        data[loop_var].button_pressed ? "Pressed" : "Released");

        //Code for maybe deleting points to keep memory here

        //Go to the next loop value
        loop_var ++;
        num_samples ++;
        cur_variance_sum = 0;
        if(num_samples >= ARRAY_SIZE){
            //Delete a value which is where the loop_var will insert the new value - point with the smallest distance from the mean

            //Calculate the mean
            double pot_sum = data[0].potentiometer_value;
            for(int i = 1; i < ARRAY_SIZE; i++){
                pot_sum += data[i].potentiometer_value;
            }
            double mean = pot_sum / ARRAY_SIZE;
            //Find the last closest value 
            int idx = 0;
            double distance = abs(data[0].potentiometer_value - mean);
            cur_variance_sum += distance;
            for(int i = 1; i < ARRAY_SIZE; i++){
                double cur_dist = abs(data[i].potentiometer_value - mean);
                if(cur_dist <= distance){
                    idx = i;
                    distance = cur_dist;
                }
                cur_variance_sum += cur_dist;
            }
            loop_var = idx; //Doesn't technically delete it, but will replace the values in data[loop_var] so it is good enough
        }
        //----------------------------------------------------------------------------------------------------

        //----------------------------------------------------------------------------------------------------
        //Housekeeping for the end of the loop
        //After all processes are complete, sleep the LED for the downtime
        gpio_put(LED_PIN, false);
        loop_end = time_us_64();
        loop_time = loop_start - loop_end;
        //----------------------------------------------------------------------------------------------------
        sleep_ms(MS_BT_LOOP);
    }

    return(0);
}