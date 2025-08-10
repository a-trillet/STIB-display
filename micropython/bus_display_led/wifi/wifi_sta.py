# wifi_sta.py
import network
import time
from bus_display_led.config import States, mac_endpoint, server_url, STATIC_FOLDER

wifi = network.WLAN(network.STA_IF)
wifi_status = {
    "ssid": None,
    "start_time": None,
    "connected": False,
    "tried": False,
    "mac": ':'.join(['%02x' % b for b in wifi.config('mac')])
}

def write_wifi_config(ssid, pswd):
    # Save SSID and password to a file
    try:
        with open('wifi_config.txt', 'w') as f:
            f.write(f"{ssid}\n{pswd}")
        print("WiFi configuration saved.")
    except Exception as e:
        print(f"Failed to save WiFi configuration: {e}")


def read_wifi_config():
    # Read SSID and password from a file
    try:
        with open('wifi_config.txt', 'r') as f:
            ssid = f.readline().strip()
            pswd = f.readline().strip()
        print("WiFi configuration read.")
        return ssid, pswd
    except Exception as e:
        print(f"Failed to read WiFi configuration: {e}")
        return None, None

def STA_connect_wifi():
    global wifi_status, state
    try:
        wifi.active(True)
        print("Connecting WiFi...")
        ssid, pswd = read_wifi_config()

        if ssid is None or pswd is None:
            print("No WiFi configuration found.")
            return 0
        
        state = States.WAITING_WIFI

        wifi_status["ssid"] = ssid
        wifi_status["start_time"] = time.time()
        wifi_status["connected"] = False
        wifi_status["tried"] = True
        raw_mac = wifi.config('mac')
        mac_str = ''.join('%02x' % b for b in raw_mac)
        wifi_status["mac"] = mac_str

        wifi.connect(ssid, pswd)
        counter = 0
        while not wifi.isconnected() and wifi.active() and counter < 30:
            time.sleep(1)
            counter += 1

        if wifi.isconnected():
            wifi_status["connected"] = True
            print("WiFi connected:", wifi.ifconfig()[0])
            from bus_display_led.wifi.wifi_ap import generate_qr_svg
            url = f"{server_url}{mac_endpoint}{wifi_status["mac"]}"
            generate_qr_svg(url, path=STATIC_FOLDER+"/mac_register.svg")
            print("QR Code SVG generated.")
            return 1
        else:
            wifi.active(False)
            print("WiFi connection failed or timeout.")
            return 0
    except Exception as e:
        print(f"Failed to connect to WiFi: {e}")
        wifi.active(False)
        return 0


