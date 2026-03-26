import speech_recognition as sr
import serial
import serial.tools.list_ports
import time

# --- CONFIGURATION ---
BAUD_RATE = 9600

def find_arduino():
    # We'll check your specific port first
    ports = [p.device for p in serial.tools.list_ports.comports()]
    if '/dev/ttyUSB0' in ports:
        return '/dev/ttyUSB0'
    # Fallback: find any USB port if the above isn't found
    for port in ports:
        if 'USB' in port or 'ACM' in port:
            return port
    return None

arduino_port = find_arduino()

if arduino_port:
    try:
        arduino = serial.Serial(arduino_port, BAUD_RATE, timeout=1)
        time.sleep(2) # Stabilization delay
        print(f"--- Connected to Arduino on {arduino_port} ---")
    except Exception as e:
        print(f"Error opening serial port: {e}")
        exit()
else:
    print("Arduino not found. Please check the USB connection.")
    exit()

def listen_for_commands():
    recognizer = sr.Recognizer()
    with sr.Microphone() as source:
        print("Calibrating for background noise...")
        recognizer.adjust_for_ambient_noise(source, duration=1)
        print("Ready! Commands: 'ON', 'OFF', 'DIM'")

        while True:
            try:
                audio = recognizer.listen(source, phrase_time_limit=3)
                text = recognizer.recognize_google(audio).lower()
                print(f"Speech Detected: {text}")

                if "on" in text:
                    arduino.write(b'V_ON\n')
                elif "off" in text or "of" in text:  # Handle common misrecognition
                    arduino.write(b'V_OFF\n')
                elif "dim" in text:
                    arduino.write(b'V_DIM\n')
                
            except sr.UnknownValueError:
                continue 
            except Exception as e:
                print(f"Error: {e}")

if __name__ == "__main__":
    listen_for_commands()