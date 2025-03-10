import network
import time
import requests
import json
from machine import Pin
from machine import Timer
import _thread

# Paramètres réseau et API
SSID = "***"
PASSWORD = "***"

# STIB API key
headers = {
    'Authorization': 'Apikey ***'
}



# Stops dictionary: name is sent in serial, and col is the LED number from left to right (1 to 12)
lines = {
    "71": {
        "row" : 1,
        "registers" : 0,
        "stopid": {
            "3556": {"name": "Delta vers CHIREC", 
                    "col": 12, "status": 1},
            "1598": {"name": "CHIREC vers Fraiteur", 
                    "col": 11, "status": 1},
            "3557": {"name": "Fraiteur vers cim d'ix", 
                    "col": 10, "status": 1},
            "3558": {"name": "Cim d'ix vers ULB",  
                    "col": 9, "status": 1},
            "3559": {"name": "ULB vers Jeanne",    
                    "col": 8, "status": 1},
            "3525": {"name": "Jeanne vers Buyl",   
                    "col": 7, "status": 1},
            "3510": {"name": "Geo Bernier vers Buyl", 
                    "col": 6, "status": 1},
            "3517": {"name": "Etang ixelles vers Geo Bernier", 
                    "col": 5, "status": 1},
            "3372": {"name": "Flagey vers Etang", 
                    "col": 4, "status": 1},
            "5611": {"name": "Musée d'XL vers Flagey", 
                    "col": 3, "status": 1},
            "2928": {"name": "Fernand Cocq vers Musée d'XL", 
                    "col": 2, "status": 1},
            "3506": {"name": "Boniface vers Fernand Cocq", 
                    "col": 1, "status": 1},
        }
    },
    "25": {
        "row" : 2,
        "registers" : 0,
        "stopid": {
            "0901": {"name": "boileau vers petillon", 
                    "col": 12, "status": 1},
            "5312": {"name": "petillon vers arsenal", 
                    "col": 11, "status": 1},
            "5311": {"name": "arsenal vers vub", 
                    "col": 10, "status": 1},
            "5266": {"name": "vub vers etterbeek",  
                    "col": 9, "status": 1},
            "5205": {"name": "Etterbeek vers roffian",    
                    "col": 8, "status": 1},
            "5200": {"name": "Roffiane vers Buyl",   
                    "col": 7, "status": 1},
            "5468": {"name": "Jeanne vers Buyl", 
                    "col": 6, "status": 1},
            "5462": {"name": "ULB vers Jeanne", 
                    "col": 5, "status": 1},
            "5461": {"name": "solbosh vers ulb", 
                    "col": 4, "status": 1},
            "5460": {"name": "marie josé vers solbosh", 
                    "col": 3, "status": 1},
            "5459": {"name": "bresil vers marie josé", 
                    "col": 2, "status": 1},
            "5451": {"name": "bondael gare vers brésil", 
                    "col": 1, "status": 1},
        }
    },
    "7": {
        "row" : 3,
        "registers" : 0,
        "stopid": {
            "0901": {"name": "boileau vers petillon", 
                    "col": 12, "status": 1},
            "5312": {"name": "petillon vers arsenal", 
                    "col": 11, "status": 1},
            "5311": {"name": "arsenal vers vub", 
                    "col": 10, "status": 1},
            "5266": {"name": "vub vers etterbeek",  
                    "col": 9, "status": 1},
            "5205": {"name": "Etterbeek vers roffian",    
                    "col": 8, "status": 1},
            "5200": {"name": "Roffiane vers Buyl",   
                    "col": 7, "status": 1},
            "5258": {"name": "cambre étoile  vers Buyl", 
                    "col": 6, "status": 1},
            "1048": {"name": "legrand vers cambre etoile", 
                    "col": 5, "status": 1},
            "5256": {"name": "bascule vers legrand", 
                    "col": 4, "status": 1},
            "5255": {"name": "Longchamp vers bascule", 
                    "col": 3, "status": 1},
            "5254": {"name": "Gossart vers Longchamp", 
                    "col": 2, "status": 1},
            "5253": {"name": "Cavell vers Gossart", 
                    "col": 1, "status": 1},
        }
    },
    "8": {
        "row" : 4,
        "registers" : 0,
         "stopid": {
            "5406": {"name": "Defacqz vers Bailli", 
                    "col": 12, "status": 1},
            "5404": {"name": "Bailli vers vleurgat", 
                    "col": 11, "status": 1},
            "5408": {"name": "vleurgat ver Abbaye", 
                    "col": 10, "status": 1},
            "5409": {"name": "Abbaye vers Legrand",  
                    "col": 9, "status": 1},
            "1047": {"name": "legrand vers cambre etoile",   
                    "col": 8, "status": 1},
            "5258": {"name": "cambre étoile  vers Buyl",   
                    "col": 7, "status": 1},
            "5468": {"name": "Jeanne vers Buyl", 
                    "col": 6, "status": 1},
            "5462": {"name": "ULB vers Jeanne", 
                    "col": 5, "status": 1},
            "5461": {"name": "solbosh vers ulb", 
                    "col": 4, "status": 1},
            "5460": {"name": "marie josé vers solbosh", 
                    "col": 3, "status": 1},
            "5459": {"name": "bresil vers marie josé", 
                    "col": 2, "status": 1},
            "5451": {"name": "bondael gare vers brésil", 
                    "col": 1, "status": 1},
        }
    }
}

# Row of LEDs; hardware resistors determine which LED it is 
# index = row, value = line number used in request
#lines_row = ["71","25"]
#lineid = "OR".join([f"lineid='{line}'" for line in lines_row])  # Adjust query syntax
#url = f"https://data.stib-mivb.brussels/api/explore/v2.1/catalog/datasets/vehicle-position-rt-production/records?select=*&where={lineid}&limit=100"
#line_ids = ["71", "25"]  # Ensure they are strings
line_ids = [key for key, value in lines.items() if value["row"] > 0]
base_url = "https://data.stib-mivb.brussels/api/explore/v2.1/catalog/datasets/vehicle-position-rt-production/records"
params = "?select=*&where=lineid='" + "'%20OR%20lineid='".join(line_ids) + "'&limit=100"

# Resulting URL
url = base_url + params

# Pin definitions
CLOCK_PIN = 18   # GPIO 18 for clock
DATA_PIN = 15    # GPIO 15 for data
LATCH_PIN = 5    # GPIO 2 for the latch pin (RCLK)
RESET_PIN = 19    # GPIO 19 for the register reset pin
OE_PIN = 2    # GPIO 35 for the register reset pin

# Pin initialization
clock   = Pin(CLOCK_PIN, Pin.OUT)
data    = Pin(DATA_PIN, Pin.OUT)
latch   = Pin(LATCH_PIN, Pin.OUT)
reset   = Pin(RESET_PIN, Pin.OUT)
oe      = Pin(OE_PIN, Pin.OUT)

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

def connect_wifi():
    wifi = network.WLAN(network.STA_IF)
    wifi.active(True)
    print("Connexion WiFi...")
    wifi.connect(SSID, PASSWORD)
    while not wifi.isconnected():
        time.sleep(1)
    print("WiFi connecté:", wifi.ifconfig()[0])

def fetch_data():
    try:
        response = requests.get(url, headers=headers)
        if response.status_code == 200:
            data = response.json()
            results = data.get("results", [])

            global stops
            for line in lines.values():
                for stop in line["stopid"].values(): 
                    stop["status"] = 0

            foundtotal = 0
            for result in results:
                lineid = str(json.loads(result.get("lineid")))
                vehicle_positions = json.loads(result.get("vehiclepositions", "[]"))
                for position in vehicle_positions:
                    point_id = position.get("pointId")
                    if point_id in lines[lineid]["stopid"].keys(): 
                        foundtotal += 1
                        lines[lineid]["stopid"][point_id]["status"] = 1
                        print("Found line "+ lineid +" stop, "+ point_id + ": " + lines[lineid]["stopid"][point_id]["name"])
            if foundtotal == 0 : 
                print("Found nothing on lines searched")
        else: 
            print("response status code: "+ response.status_code)

    except Exception as e:
        print(f"Erreur: {type(e).__name__} - {e}")


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

def update_leds():
    # Update LEDs based on stop statuses
    max_row = max(line_data["row"] for line_data in lines.values())
    reg_list = [0] * max_row
    #for stop_id, stop in stops.items():
    #    if stop["status"] == 1:
    #        lines_dico[stop["row"]] |= led_to_register[stop["col"]]
    for line in lines.values(): 
        line["registers"] = 0
        for stop in line["stopid"].values(): 
            if stop["status"] == 1:
                line["registers"] |= led_to_register[stop["col"]]
        if line["row"] > 0 : 
            reg_list[line["row"]-1] = line["registers"]
        reg_list
    
    for i in range(0,max_row):
        print(i)
        feed_register(reg_list[i])

    # latch the registered values to the LEDs for all lines
    pulse(latch)

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

    

def main():
    init_pins()
    update_leds()  # sets all led to inital value (ideally status = 1 to make sure they are all working)
    start_time = time.ticks_ms()
    connect_wifi()
    # wait while timer has not lasted 2seconds so that all leds stay on for 2 sec at least at each reboot
    while time.ticks_diff(time.ticks_ms(), start_time) < 2000:
        pass  # Do nothing, just wait for 2 seconds

    while True:
        fetch_data()

        update_leds()

        time.sleep(20)

if __name__ == "__main__":
    main()
