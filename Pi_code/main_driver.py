#!usr/bin/python

import serial
import subprocess, sys

def wait_for_usb_devices(num_devices_left, whitelist):
    # Code will break when the new usb device is detected
    new_device = False

    if num_devices_left == 0:
        return []
    
    picos = []

    #Loop through and only break when there is a new usb device
    while not new_device:
        
        #Get the usb devices
        result = subprocess.check_output("lsusb", text = True)
        devices = result.split('\n')
        num_devices = len(devices)
        #If the last 'device' is just a '', get rid of it
        if devices[num_devices - 1] == "":
            devices = devices[0 : num_devices - 1]
            num_devices = num_devices - 1
        # print(num_devices, end = "\n")
        # print(devices)
        
        #Determine if there is a new device plugged in
        
        for device in devices:
            if device[23 : 32] not in whitelist:
                new_device = True
                whitelist.append(device[23 : 32])
                picos.append(device[23 : 32])
                picos = picos + wait_for_usb_devices(num_devices_left - 1, whitelist)
                print("New Device Detected: %s", device)
                break
    return picos
# ============================================================

# ============================================================


# ============================================================
#Start the main block# Current whitelist - take from Pi
whitelist = ['1d6b:0003',
            '046d:c534',
            '04d9:0006',
            '05e3:0610',
            '2109:3431',
            '1d6b:0002']

picos = wait_for_usb_devices(6, whitelist)
print("All devices found!")
# try:
#     ser = serial.Serial('/dev/ttyACM0', 9600, timeout = 10)
#     print("Connection successful")
# except:
#     print("Device not connected")
# #listen for input like on Google
# while True:
#     line = ser.readline()
#     if len(line) == 0:
#         print("Nothing to see here")
#     else:
#         print(line)