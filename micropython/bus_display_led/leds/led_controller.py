# led_controller.py
import time
from machine import Pin
import urequests as requests
from bus_display_led.config import server_url, ledstrip_endpoint
from bus_display_led.wifi.wifi_sta import wifi_status


# Pin definitions
CLOCK_PIN = 18   # GPIO 18 for clock
DATA_PIN = 15    # GPIO 15 for data
LATCH_PIN = 5    # GPIO 2 for the latch pin (RCLK)
RESET_PIN = 19    # GPIO 19 for the register reset pin
OE_PIN = 2    # GPIO 35 for the register reset pin

# Mapping LEDs to register values
led_to_register = {
    1: 0b0100000000000000,
    2: 0b0010000000000000,
    3: 0b0001000000000000,
    4: 0b0000100000000000,
    5: 0b0000010000000000,
    6: 0b0000001000000000,
    7: 0b0000000000000010,
    8: 0b0000000000000100,
    9: 0b0000000000001000,
    10: 0b0000000000010000,
    11: 0b0000000000100000,
    12: 0b0000000001000000,
}

# Pin initialization
clock   = Pin(CLOCK_PIN, Pin.OUT)
data    = Pin(DATA_PIN, Pin.OUT)
latch   = Pin(LATCH_PIN, Pin.OUT)
reset   = Pin(RESET_PIN, Pin.OUT)
oe      = Pin(OE_PIN, Pin.OUT)

def pulse(pin):
    """
    Generates a clock pulse on the given pin. (+- 10kHz, max 15MHz)
    """
    time.sleep_us(25)
    pin.on()
    time.sleep_us(50)
    pin.off()
    time.sleep_us(25)


def feed_register(leds):
    """
    Sends a 16-bit value to the shift register, LSB first.

    :param row: The row number (1-4).
    :param leds: A 16-bit integer representing the LED states.
    """
    # Send each bit (from LSB to MSB)
    for i in range(16):
        bit = (leds >> i) & 1  # Extract the bit (starting from LSB)
        data.value(bit)       # Set the data line
        pulse(clock)     # Pulse the clock pin
    data.value(0)             # reset to 0 in idle state


def init_pins():
    print("Initialising pins")
    oe.on() #disable output (high impedance)
    latch.off()
    clock.off()
    data.off() 
    pulse(reset) # put low to reset shift register 
    reset.on() # keep on to prevent reseting the registers
    pulse(latch) # put 0s in latches
    oe.off() # keep down to enable output 

def update_leds(value):
    """ Update the LEDs by sending a value to the shift register."""
    feed_register(led_to_register[value])
    pulse(latch)  # Latch the data into the output register


# Fetch data and update LEDs
def fetch_update_leds():
    """
    Fetch LED strip status and update shift registers.
    """
    try:
        global last_response
        url = f"{server_url}{ledstrip_endpoint}{wifi_status['mac']}"
        last_response = requests.get(url, timeout=5)
        if last_response.status_code != 200:
            print("Error:", last_response.json())
            return last_response.status_code

        data = last_response.json()
        strips = data.get('strips', [])
        for strip in strips:
            leds = strip.get('v', [])
            register_value = 0
            for i, val in enumerate(leds):
                if val and (i + 1) in led_to_register:
                    register_value |= led_to_register[i + 1]
            feed_register(register_value)
        pulse(latch)
        return last_response.status_code
    
    except Exception as e:
        print("Exception:", e)
        return -1
