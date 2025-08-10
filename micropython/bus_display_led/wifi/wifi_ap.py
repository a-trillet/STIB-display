# wifi_ap.py
import network
import socket
import ure
import time
import gc
import os
from bus_display_led.config import ssid_AP, States, server_url, mac_endpoint, STATIC_FOLDER
from bus_display_led.wifi.wifi_sta import write_wifi_config, wifi_status, wifi
from bus_display_led.wifi.uQR import QRCode

def generate_qr_svg(data, size=200, path="/mac_register.svg"):
    # Skip if file already exists
    try:
        if path in os.listdir("/"):
            return
    except OSError:
        # Directory doesn't exist, ignore and proceed
        pass

    qr = QRCode()
    qr.add_data(data)
    matrix = qr.get_matrix()
    n = len(matrix)
    scale = size // n or 1

    with open(path, "w") as f:
        _ = f.write(f'<svg width="{size}" height="{size}" xmlns="http://www.w3.org/2000/svg">\n')
        _ = f.write('<rect width="100%" height="100%" fill="white"/>\n')
        for y, row in enumerate(matrix):
            for x, cell in enumerate(row):
                if cell:
                    _ = f.write(f'<rect x="{x*scale}" y="{y*scale}" '
                                f'width="{scale}" height="{scale}" fill="black"/>\n')
        _ = f.write('</svg>\n')

    gc.collect()



def AP_web_page():
    global wifi_status, state
    ssid = wifi_status["ssid"]
    connected = wifi_status["connected"]
    start_time = wifi_status["start_time"]
    tried = wifi_status["tried"]
    mac = wifi_status["mac"]
    registration_url = f"{server_url}{mac_endpoint}{wifi_status["mac"]}"

    def get_status_msg():
        if tried and ssid:
            elapsed = int(time.time() - start_time) if start_time else 0
            if connected:
                return f"Connected to <b>{ssid}</b>."
            elif elapsed < 15:
                return f"Connecting to <b>{ssid}</b>... ({elapsed}s)"
            else:
                return f"Failed to connect to <b>{ssid}</b> after {elapsed} seconds."
        return "No connection attempt yet."

    wifi_status_html = f'<div id="status"><h2>{get_status_msg()}</h2></div>'
    wifi_html = f"""
    <h1>Enter your Wi-Fi credentials (they will be stored locally only)</h1>
    <form action="/apply" method="post">
        <label for="ssid">Wi-Fi name (SSID):</label><br>
        <input type="text" id="ssid" name="ssid"><br><br>
        <label for="pswd">Wi-Fi password:</label><br>
        <input type="text" id="pswd" name="pswd"><br><br>
        <input type="submit" value="Submit">
    </form>
    """
    register_html = f"""
    <hr>
    <h2>Register this device online</h2>
    <p><b>Important:</b> Because this Wi-Fi has no internet, your phone may block the link below.</p>
    <p>Please turn off Wi-Fi (or open the link using mobile data) to register your device:</p>
    <a href="{registration_url}" target="_blank">
        <button style="font-size: 20px; padding: 10px;">Register device</button>
    </a>
    <p>You can also scan this QR code:</p>
    <img src="/mac_register.svg" alt="QR Code" width="200" height="200">
    """

    html = f"""<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="/style.css">
    <script>
        function refreshStatus() {{
            fetch("/status").then(r => r.text()).then(d => {{
                document.getElementById("status").innerHTML = d;
            }});
        }}
        setInterval(refreshStatus, 3000);
    </script>
</head>
<body>
    {wifi_status_html}
    {wifi_html}
    <p><b>Device MAC:</b> {mac}</p>
    {register_html}
</body>
</html>"""
    return html




def AP_mode_init(): 
    global state 
    state = States.STARTING_AP
    print("Setting up AP mode...")
    ap = network.WLAN(network.AP_IF)
    ap.active(True)
    ap.config(essid=ssid_AP, authmode=network.AUTH_OPEN)

    while ap.active() == False:
        pass
    print('AP mode setup successful')
    print(ap.ifconfig())


def AP_mode_terminate():
    print("Terminating AP mode...")
    ap = network.WLAN(network.AP_IF)
    ap.active(False)
    while ap.active():
        pass
    print("AP mode terminated.")


def AP_mode_socket():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('', 80))
    s.listen(5)
    try:
        while True:
            conn, addr = s.accept()
            print('Got a connection from %s' % str(addr))
            request = conn.recv(1024).decode()
            if not request or len(request.strip()) == 0:
                conn.close()
                continue

            # Optional: log only valid, non-icon requests for clarity
            if 'favicon.ico' not in request:
                print('Content = %s' % str(request))
            
            apply = False
            if 'POST /apply' in request:
                apply = True
                parts = request.split('\r\n\r\n')[1]
                post_data = parts[1] if len(parts) > 1 else ''
                ssid_match = ure.search(r'ssid=([^&]*)', post_data)
                password_match = ure.search(r'pswd=([^&]*)', post_data)
                ssid = ssid_match.group(1) if ssid_match else ''
                pswd = password_match.group(1) if password_match else ''
                print(f"Credentials received: \nSSID: {ssid}\nPassword: {pswd}")

                write_wifi_config(ssid, pswd)
                wifi_status["ssid"] = ssid
                wifi_status["start_time"] = time.time()
                wifi_status["connected"] = False
                wifi_status["tried"] = True

                response = AP_web_page()
            elif 'GET /style.css' in request or 'GET /mac_register.svg' in request:
                path = ''
                if 'GET /style.css' in request:
                    path = STATIC_FOLDER + "/style.css"
                    content_type = 'text/css'
                elif 'GET /mac_register.svg' in request:
                    path = STATIC_FOLDER + "/mac_register.svg"
                    content_type = 'image/svg+xml'

                try:
                    conn.send('HTTP/1.1 200 OK\n')
                    conn.send(f'Content-Type: {content_type}\n')
                    conn.send('Cache-Control: public, max-age=86400\n')
                    conn.send('Connection: close\n\n')
                    with open(path, 'r') as f:
                        while True:
                            data = f.read(512)
                            if not data:
                                break
                            conn.send(data)
                except OSError:
                    conn.send('HTTP/1.1 404 Not Found\n')
                    conn.send('Connection: close\n\n')
                    conn.sendall(f"File {path} not found.")
                conn.close()
                continue
            elif 'GET /favicon.ico' in request:
                # Return a 204 No Content or dummy response
                conn.send('HTTP/1.1 204 No Content\n')
                conn.send('Connection: close\n\n')
                conn.close()
                continue
            elif 'GET /status' in request:
                ssid = wifi_status["ssid"]
                connected = wifi_status["connected"]
                start_time = wifi_status["start_time"]
                tried = wifi_status["tried"]
                elapsed = int(time.time() - start_time) if start_time else 0

                if tried and ssid:
                    if connected:
                        status_msg = f"Connected to <b>{ssid}</b>."
                    elif elapsed < 15:
                        status_msg = f"Connecting to <b>{ssid}</b>... ({elapsed}s)"
                    else:
                        status_msg = f"Failed to connect to <b>{ssid}</b> after {elapsed} seconds."
                else:
                    status_msg = "No connection attempt yet."

                conn.send('HTTP/1.1 200 OK\n')
                conn.send('Content-Type: text/html\n')
                conn.send('Connection: close\n\n')
                conn.sendall(f"<h2>{status_msg}</h2>")
                conn.close()
                continue  # skip rest of the loop
            else:
                response = AP_web_page()
            
            conn.send('HTTP/1.1 200 OK\n')
            conn.send('Content-Type: text/html\n')
            conn.send('Connection: close\n\n')
            conn.send(response)
            conn.close()
            
            if apply == True : 
                wifi.active(False) # turn off STA mode
                write_wifi_config(ssid, pswd)
                # sta connect will stop and be restarted by main loop as wifi.isconnected = False
    finally:
        s.close()
