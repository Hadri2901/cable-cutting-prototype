import serial
import time
from pynput import keyboard

# Replace with your actual port
ser = serial.Serial('/dev/cu.usbmodem1101', 9600)
time.sleep(2)

print("Hold W to go up, S to go down. Release = stop. Press ESC to exit.")

pressed_keys = set()

def send_command(cmd):
    ser.write(cmd.encode())

def on_press(key):
    try:
        if key.char == 'w':
            if 'w' not in pressed_keys:
                pressed_keys.add('w')
                send_command('w')
        elif key.char == 's':
            if 's' not in pressed_keys:
                pressed_keys.add('s')
                send_command('s')
    except AttributeError:
        pass  # Special keys

def on_release(key):
    try:
        if key.char in ['w', 's']:
            pressed_keys.discard(key.char)
            if not pressed_keys:
                send_command('0')
    except AttributeError:
        if key == keyboard.Key.esc:
            send_command('x')
            ser.write(b'0')
            ser.close()
            print("Exited.")
            return False

listener = keyboard.Listener(on_press=on_press, on_release=on_release)
listener.start()
listener.join()
