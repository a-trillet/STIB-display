import machine
import sys
import gc
import time
import os

SLOT_A = "slot_a"
SLOT_B = "slot_b"
ACTIVE_FILE = "active_slot.txt"

def logerror(e=None):
    with open('error.log', 'w') as f:
        if e:
            f.write(str(e) + '\n')
        else:
            f.write('Unknown error\n')
    with open('error_sent', 'w') as f:
        f.write('1')

def get_active_slot():
    if ACTIVE_FILE in os.listdir():
        with open(ACTIVE_FILE, 'r') as f:
            slot = f.read().strip()
            if slot in (SLOT_A, SLOT_B):
                return slot
    return SLOT_A  # default if file missing

def set_active_slot(slot):
    with open(ACTIVE_FILE, 'w') as f:
        f.write(slot)

def other_slot(slot):
    return SLOT_B if slot == SLOT_A else SLOT_A

def try_run(slot):
    print(f"=== Booting from {slot} ===")
    sys.path.insert(0, slot)
    try:
        from bus_display_led import main as app_main
        result = app_main.main()
        print(f"{slot} exited with return value:", result)
        logerror(f"{slot} returned: {result}")
        return False
    except Exception as e:
        print(f"Error in {slot}:", e)
        logerror(e)
        return False
    finally:
        sys.path.pop(0)

def safe_boot():
    active = get_active_slot()
    inactive = other_slot(active)

    if try_run(active):
        print(f"{active} ran successfully.")
        return

    print(f"{active} failed — trying {inactive}...")
    if try_run(inactive):
        print(f"{inactive} ran successfully — setting it as active.")
        set_active_slot(inactive)
        return

    print("Both slots failed! Waiting for manual intervention.")

if __name__ == "__main__":
    safe_boot()
    sleep_time = 10
    print(f"Sleeping for {sleep_time} seconds before rebooting...")
    time.sleep(sleep_time)
    print("Rebooting...")
    gc.collect()
    machine.reset()
