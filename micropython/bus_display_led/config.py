# config.py
# wifi credentials (soft AP mode)
ssid_AP = 'Bus-Display-LED'
server_url = 'https://transport.trillet.be'
mac_endpoint = '/devices/register_new_device?mac='
ledstrip_endpoint = '/api/esp/ledstrips?mac='
version_endpoint = '/api/update/versions'
app_version = '1.0.0'


STATIC_FOLDER = "/bus_display_led/static"

# States
class States:
    STARTING_AP = 1
    WAITING_WIFI = 2
    CONTACTING_SERVER = 3
    NORMAL = 4
    UNKNOWN_MAC = 5
    NO_BOARD = 6
    ERROR = 7

