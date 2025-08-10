# bus_display_led/updater/updater.py

import urequests as requests
import os
import machine
import gc
import time

from bus_display_led.config import server_url, version_endpoint, app_version
from bus_display_led.wifi.wifi_sta import wifi_status

ACTIVE_FILE = "active_slot.txt"
SLOT_A = "slot_a"
SLOT_B = "slot_b"


def get_active_slot():
    try:
        with open(ACTIVE_FILE, 'r') as f:
            slot = f.read().strip()
            if slot in (SLOT_A, SLOT_B):
                return slot
    except:
        pass
    return SLOT_A  # default

def other_slot(slot):
    return SLOT_B if slot == SLOT_A else SLOT_A

def clear_directory(path):
    try:
        import shutil
        shutil.rmtree(path)
    except:
        try:
            for entry in os.ilistdir(path):
                full_path = f"{path}/{entry[0]}"
                if entry[1] == 0x4000:  # dir
                    clear_directory(full_path)
                    os.rmdir(full_path)
                else:
                    os.remove(full_path)
        except OSError:
            pass


def mkdir_p(path):
    parts = path.split(os.sep)
    for i in range(1, len(parts)+1):
        subpath = os.sep.join(parts[:i])
        try:
            os.mkdir(subpath)
        except OSError:
            pass  # Already exists or cannot create

def download_and_extract(url, dest):
    print(f"Downloading update from {url}...")
    try:
        r = requests.get(url, stream=True)
    except Exception as e:
        print("Download failed to start:", e)
        return False

    try:
        if r.status_code != 200:
            print("Download failed:", r.status_code)
            return False

        tar_path = "update.tar"

        # Save file in chunks
        try:
            with open(tar_path, "wb") as f:
                while True:
                    chunk = r.raw.read(512)
                    if not chunk:
                        break
                    f.write(chunk)
        except Exception as e:
            print("Error saving file:", e)
            return False

        # Minimal tar extraction
        try:
            print("Extracting update (tar)...")
            with open(tar_path, "rb") as f:
                while True:
                    header = f.read(512)
                    if len(header) < 512:
                        break  # End of archive or corrupt

                    # Check for two consecutive 512-byte blocks of zeroes marking end of archive
                    if header == b'\0' * 512:
                        next_block = f.read(512)
                        if next_block == b'\0' * 512:
                            break
                        else:
                            f.seek(-512, 1)  # Rewind one block and continue

                    # Parse file name
                    name = header[0:100].rstrip(b'\0').decode()
                    if not name:
                        break

                    # File size is stored as octal string, strip null and space
                    size_str = header[124:136].rstrip(b'\0 ').decode()
                    size = int(size_str, 8) if size_str else 0

                    # File type flag (0 or '\0' = normal file, '5' = directory)
                    typeflag = header[156:157]

                    full_path = f"{dest}{os.sep}{name}"

                    if typeflag == b'5':
                        # Directory
                        try:
                            mkdir_p(full_path)
                        except Exception:
                            pass
                    elif typeflag in (b'0', b'\0'):
                        # Regular file
                        dir_path = os.sep.join(full_path.split(os.sep)[:-1])
                        try:
                            mkdir_p(dir_path)
                        except Exception:
                            pass

                        with open(full_path, "wb") as outfile:
                            remaining = size
                            while remaining > 0:
                                to_read = 512 if remaining >= 512 else remaining
                                data = f.read(512)
                                outfile.write(data[:to_read])
                                remaining -= to_read
                    else:
                        # Unsupported type, skip
                        pass

                    # Skip padding to next 512-byte block
                    if size % 512 != 0:
                        to_skip = 512 - (size % 512)
                        f.seek(to_skip, 1)

        except Exception as e:
            print("Error extracting tar:", e)
            try:
                os.remove(tar_path)
            except:
                pass
            return False

        try:
            os.remove(tar_path)
        except OSError:
            pass

        return True

    finally:
        r.close()

def get_hardware_version():
    try:
        with open("HARDWARE_VERSION.txt", "r") as f:
            version = f.read().strip()
            if version:
                return version
    except OSError:
        pass
    # Fallback default hardware version if file missing or empty
    return "unknown"

def check_for_updates():
    active_slot = get_active_slot()
    inactive_slot = other_slot(active_slot)

    payload = {
        "hardware": get_hardware_version(),
        "mac": wifi_status['mac'],
        "app_version": app_version
    }

    try:
        print("Checking for update...")
        r = requests.post(f"{server_url}{version_endpoint}", json=payload)
        if r.status_code != 200:
            print("Server error:", r.status_code)
            return
        resp = r.json()
        r.close()

        if "app_url" in resp and resp["app_url"]:
            print("Update available! Installing to", inactive_slot)
            gc.collect()

            # Clear old inactive slot
            clear_directory(inactive_slot)

            # Download & extract
            if download_and_extract(resp["app_url"], inactive_slot):
                # Switch active slot
                with open(ACTIVE_FILE, 'w') as af:
                    af.write(inactive_slot)

                print("Update installed. Rebooting into new slot...")
                time.sleep(2)
                gc.collect()
                machine.reset()
            else:
                print("Update failed to install.")
        else:
            print("Already up to date.")

    except Exception as e:
        print("Update check failed:", e)
