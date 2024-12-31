import network
import time
import requests
import json
from machine import Pin
import _thread

# Configuration des broches GPIO pour multiplexeur
S0 = Pin(18, Pin.OUT)
S1 = Pin(5, Pin.OUT)
S2 = Pin(4, Pin.OUT)
S3 = Pin(2, Pin.OUT)
SIG = Pin(19, Pin.OUT)

# Initialiser les broches
for pin in [S0, S1, S2, S3, SIG]:
    pin.value(0)

# Paramètres réseau et API (change **** with your Wifi and Stib API credentials)
SSID = "****"
PASSWORD = "****"
url = "https://data.stib-mivb.brussels/api/explore/v2.1/catalog/datasets/vehicle-position-rt-production/records?select=*&where=lineid%3D71&limit=100"
headers = {
    'Authorization': 'Apikey ****'
}

# Configuration des points
target_point_ids = ["3558", "3559", "3525", "2351", "2397", "3510", "3517", "3372"]
point_names = {
    "3558": "Cim d'ix vers ULB 3558",
    "3559": "ULB vers Jeanne 3559",
    "3525": "Jeanne vers Buyl 3525",
    "2351": "Buyl(debroucker) vers Geo Bernier2351",
    "2397": "Buyl(delta) vers ULB 2397",
    "3510": "Geo Bernier vers Buyl 3510B",
    "3517": "Etang ixelles vers Geo Bernier 3517",
    "3372": "Flagey vers Etang 3372"
}

point_to_channel = {
    "3558": 6,
    "3559": 5,
    "3525": 4,
    "2351": 3,
    "2397": 3,
    "3510": 2,
    "3517": 1,
    "3372": 0
}

# État des LEDs
active_leds = {channel: False for channel in range(8)}
led_lock = _thread.allocate_lock()

def connect_wifi():
    wifi = network.WLAN(network.STA_IF)
    wifi.active(True)
    print("Connexion WiFi...")
    wifi.connect(SSID, PASSWORD)
    while not wifi.isconnected():
        time.sleep(1)
    print("WiFi connecté:", wifi.ifconfig()[0])

def select_channel(channel):
    S0.value(channel & 0x01)
    S1.value((channel >> 1) & 0x01)
    S2.value((channel >> 2) & 0x01)
    S3.value((channel >> 3) & 0x01)

def led_multiplexing():
    """Thread pour gérer le multiplexage des LEDs"""
    while True:
        with led_lock:
            for channel, is_active in active_leds.items():
                if is_active:
                    select_channel(channel)
                    SIG.value(1)
                    time.sleep(0.001)  # Petit délai pour que la LED soit visible
                    SIG.value(0)

def fetch_data():
    try:
        response = requests.get(url, headers=headers)
        if response.status_code == 200:
            data = response.json()
            results = data.get("results", [])
            found_points = []

            for result in results:
                vehicle_positions = json.loads(result.get("vehiclepositions", "[]"))
                for position in vehicle_positions:
                    point_id = position.get("pointId")
                    if point_id in target_point_ids:
                        found_points.append(point_id)
                        point_name = point_names.get(point_id, "Inconnu")
                        print(f"Bus à: {point_id} - {point_name}")
                        
                        channel = point_to_channel.get(point_id)
                        if channel is not None:
                            with led_lock:
                                active_leds[channel] = True
                            print(f"LED {channel} activée")

            # Mettre à jour l'état des LEDs pour les points non trouvés
            with led_lock:
                for point_id, channel in point_to_channel.items():
                    if point_id not in found_points and channel in active_leds:
                        active_leds[channel] = False

    except Exception as e:
        print(f"Erreur: {e}")

def main():
    connect_wifi()
    # Démarrer le thread de multiplexage
    _thread.start_new_thread(led_multiplexing, ())
    
    while True:
        fetch_data()
        time.sleep(20)

if __name__ == "__main__":
    main()