# MinisculeDB
Advanced Data Systems Implementation Project mimicking principles in TinyDB

## Pico Code
- This contains code to be copied to be loaded onto all of the picos. To set up my Pico SDK, I followed the Shawn Hymel tutorial page and video listed below.
    - https://shawnhymel.com/2096/how-to-set-up-raspberry-pi-pico-c-c-toolchain-on-windows-with-vs-code/
    - https://www.youtube.com/watch?v=B5rQSoOmR5w
- After creating the toolchain, simply run the command:
```
cmake -G "MinGW Makefiles" . -B build
```
- Once the documents are created run `make`.
- Press the bootsel button on the Pico and plug it into the computer. This will put the Pico into Bootloader mode and show as a flash drive on your machine. Copy the .uf2 file into this drive.

## Pi Code
Pending

