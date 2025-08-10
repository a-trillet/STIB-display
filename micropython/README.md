# Erase flash first
python -m esptool --chip esp32 --port COM3 erase_flash

# Flash the OTA upython firmware
python -m esptool --chip esp32 --port COM3 --baud 460800 write_flash -z 0x1000 .\arduino-esp32\ESP32_GENERIC-OTA-20250415-v1.25.0.bin

# Flash the compile python code
mpremote connect COM3 cp -r bus_display_led\ :
mpremote connect COM3 cp main.py :

ref: 5c:01:3b:67:9c:ec
import bus_display_led.main as app_main