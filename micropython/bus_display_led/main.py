# main.py
from bus_display_led.wifi.wifi_sta import STA_connect_wifi, wifi
from bus_display_led.leds.led_controller import init_pins, update_leds, fetch_update_leds
from bus_display_led.config import States
from bus_display_led.wifi.wifi_ap import AP_mode_init, AP_mode_socket
from bus_display_led.updater.updater import check_for_updates
import gc 
import time
import _thread

def main():
    init_pins()
    time.sleep(2)

    # Start AP mode 
    state = States.STARTING_AP
    print("Starting AP mode...")
    update_leds(state)
    AP_mode_init()
    gc.collect()
    # Start AP mode socket in a separate thread
    _thread.start_new_thread(AP_mode_socket, ()) 
    loop_counter = 0

    while True:
        status_code = None
        gc.collect()
        if not wifi.isconnected():
            state = States.WAITING_WIFI
            print("Waiting for Wi-Fi connection...")
            update_leds(state)
            STA_connect_wifi() # start loop waiting for connection success (can be interrupted by failure or wifi.active(False))
            time.sleep(2) # wait two seconds before retrying
        else : 
            if status_code is None or status_code != 200:
                state = States.CONTACTING_SERVER
                print("Contacting server...")
                
            status_code = fetch_update_leds()

            if status_code == 200:
                print("LEDs updated successfully.")
                state = States.NORMAL

            elif status_code == 401:
                state = States.UNKNOWN_MAC
                print("Unknown MAC address. Please register your device.")
                update_leds(state)

            elif status_code == 404:
                state = States.NO_BOARD
                print("No board found for this MAC address.")
                update_leds(state)

            else:
                state = States.ERROR
                update_leds(state)
            
            if loop_counter == 0:
                print("Checking for updates...")
                check_for_updates()
                loop_counter = 100

            loop_counter -= 1
            time.sleep(10)


if __name__ == "__main__":
    main()
