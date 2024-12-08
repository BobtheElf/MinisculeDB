# MinisculeDB
Advanced Data Systems Implementation Project mimicking principles in TinyDB

## Pico Code

### Building and Running
- This contains code to be copied to be loaded onto all of the Picos. To set up my Pico SDK, I followed the Shawn Hymel tutorial page and video listed below.
    - https://shawnhymel.com/2096/how-to-set-up-raspberry-pi-pico-c-c-toolchain-on-windows-with-vs-code/
    - https://www.youtube.com/watch?v=B5rQSoOmR5w
- After creating the toolchain eidt the cmake file to include these libraries in the `target_link_libraries` section:
```
pico_stdlib
hardware_adc
pico_rand
pico_time
```
- Ensure you have enabled stdio_usb and disabled stdio_uart in the cmake file as well.
- Now, run the command:
```
cmake -G "MinGW Makefiles" . -B build
```
- Once the documents are created run `make`.
- Press the bootsel button on the Pico and plug it into the computer. This will put the Pico into Bootloader mode and show as a flash drive on your machine. Copy the .uf2 file into this drive.

### DB Attributes
This database runs on a single PICO, and most changes to make this Pico more optimized to your need will have to be made in the source code for now.

In terms of hardware, the Pico code is written to set up a potentiometer pin on pin 26, push-button pin on pin 15, and LED pin on pin 25. You can change these pinouts to other pins, but make sure that the potentiometer pin is connected to an ADC pin and make sure that a wired LED pin does not use pin 25, as that is the onboard LED. They are set to adc, pull-up resistor, and output pins respectively in the settings.

The database is 500 elements long which is adjustable to your liking by editing the ARRAY_SIZE constant. Messages over serial to the Pico will be received in 128 byte chunks, although all of the information is eventually processed. The code stores DataPoint structures as a part of its functioning, and to add more inputs or fields to the database simply add more fields in the struct to be loaded by hardware sensors in the code.

In order to access elements of the database, you first need to learn the query language. It is very exact, and any variations to the syntax will result in unpredictable results, as the Pico is operating under the assumption that another machine with a better query syntax generater is querying the system. See `Pico_code/practice_query.txt` for example queries. Below is the grammar to query the database:
```
Query: SELECT [var](,[var])?(,[var])?(,[var])?( WHERE [var][op][value])( ORDER BY [var](,[var])?(,[var])?(,[var])?)?
[var]: time, potv, butp, ledo
[op]: <, >, =, <=, >=, !=
[value]: [0-9]+
```
For every loop in the Pico code, the Pico code does some start-of-loop housekeeping, reads from the serial port, interprets the message if there is one, parses SQL logic if applicable, collects data from the Pico sensors and pins, and finally does some end-of-loop housekeeping. I will describe what each section does in detail.

#### Start of Loop Housekeeping
For the start of the loop, the code first initializes variables that help the rest of the code run by keeping time, determining how large the database is, and finally turning on the on-board LED so that the use knows that the system is working.

#### Read from Serial
In this block, the Pico uses buffers along with helper functions to get a 128 byte block of data from the serial port. The data at the serial port only changes when there is new data sent, so the code ensures that the Pico only responds to the serial input when the data on the serial port is different than a buffer of previously stored data.

#### Interpreting the Message
If there is not new data on the serial port, this section is skipped, but if there is new data, the code determines if the message fits into one of 6 different message types described below.

- HELO: prints the Pico's randomly chosen device id, the timestamp, and a message back that reads "EHLO" - useful for broadcasting identifying information as well as readiness for another task.
- TIME: prints the time taken for the previous loop - useful for synchronizing time epochs - this code is broken which is interesting because neither me nor the code can find the syntactic errors that lead to the bugginess of the functionality
- DUMP: prints all of the data in the database in the order it is stored in - useful for debugging or for a simple SELECT * query without any frills.
- SELECT: this is a query statement, and gets lexed into tokens that are used in parsing the query - quite error prone if you are not careful with the syntax, however does successfully lex well-formed queries into tokens that are usable by the parser
- PAUSE: halts data collection on the Pico without halting the serial connection - every other kind of statement works while data collection is paused, and this is a great way to get a snapshot of the data in the Pico
- GO: resumes data collection on the Pico - useful for breaking out of a debugging session smoothly

#### Parsing the Query
Assuming there is a query, this section takes the tokens lexed by the message interpreting section and queries the array for data. This obeys the smallest bit of relational algebra in that it processes the WHERE clause first, the ORDER BY clause second, and the SELECT clause last so that it is operating on as little data as possible. Each clause works like a traditional relational database where the WHERE clause filters data, the ORDER BY clause orders data, and the SELECT clause projects and returns data. To return the data, the SELECT section just prints rows of data to serial output. This part is also buggy sometimes for reasons I have not been able to find, but this version is the least buggy of the entire project. The last part of this section is just code housekeeping to reset all of the query tokens so that the query is not run more than once per input on the data array.

#### Collecting Data from the Pico
Assuming that the Pico has not been paused, this section reads all of the sensors and values one-by-one, putting them into their respective data fields at a loop index that is determined as follows. When the Pico has fewer than ARRAY_SIZE data values, the loop variable increases from 0 to ARRAY_SIZE. After reaching ARRAY_SIZE, the code picks a data value to delete to preserve the set number of array values. To pick this value, the program takes the mean of the potentiometer values and sets the loop variable to the index where the data point's potentiometer value is closest to the mean. This way, the data maintains the most extreme values, and can record significant events over time more easily without losing too much information.

#### End of Loop Housekeeping
To end the code loop, this section turns off the LED that has been on for the whole loop, records the time for variables that need the time, and then sleeps the processor for MS_BT_LOOP milliseconds. This variable forces the Pico to take longer in order to chart slower changes for the data collected.

## Pi Code
Pending - there is a script that finds a set number of Picos over usb and then quits. Run this script at your own risk.

