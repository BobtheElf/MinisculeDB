import serial
import glob

def find_usb_devices(whitelist):
    """
    Finds all USB serial devices connected to the system.
    Returns a list of device paths.
    """
    return (glob.glob('/dev/ttyUSB*') - whitelist)

def send_message_to_devices(devices, message):
    """
    Sends a message to all connected USB devices.
    
    Args:
    devices: List of device paths.
    message: The message to send (string).
    """
    for device in devices:
        try:
            with serial.Serial(device, baudrate=115200, timeout=1) as ser:
                ser.write(message.encode('utf-8'))
                print(f"Message sent to {device}")
        except Exception as e:
            print(f"Failed to send message to {device}: {e}")

if __name__ == "__main__":
    print("Assuming all devices are already plugged into the Raspberry Pi via usb.")
    whitelist = ['1d6b:0003',
            '046d:c534',
            '04d9:0006',
            '05e3:0610',
            '2109:3431',
            '1d6b:0002']
    usb_devices = find_usb_devices(whitelist)
    if not usb_devices:
        print("No USB devices found.")
    else:
        print(f"Found USB devices: {usb_devices}")
        send_message_to_devices(usb_devices, "hello, world")
