import speech_recognition as sr
import serial
import time

# --- CONFIGURATION ---
# Replace 'COM3' with the port you found in the Arduino IDE
ARDUINO_PORT = 'COM3' 
BAUD_RATE = 9600

try:
    arduino = serial.Serial(ARDUINO_PORT, BAUD_RATE, timeout=1)
    time.sleep(2) # Give Arduino time to reset
    print(f"Connected to Arduino on {ARDUINO_PORT}")
except:
    print("Could not connect to Arduino. Check the Port.")
    exit()

def listen_for_commands():
    recognizer = sr.Recognizer()
    # Adjust for ambient noise
    with sr.Microphone() as source:
        print("Adjusting for background noise... please wait.")
        recognizer.adjust_for_ambient_noise(source, duration=1)
        print("Ready! Say On,  Off, or Dim...")

        while True:
            try:
                audio = recognizer.listen(source, phrase_time_limit=3)
                text = recognizer.recognize_google(audio).lower()
                print(f"Heard: {text}")

                if "on" in text:
                    arduino.write(b'V_ON\n')
                    print("Sent: ON")
                elif "off" in text:
                    arduino.write(b'V_OFF\n')
                    print("Sent: OFF")
                elif "dim" in text:
                    arduino.write(b'V_DIM\n')
                    print("Sent: DIM")
                
            except sr.UnknownValueError:
                # Occurs when the AI hears sound but can't find words
                continue 
            except sr.RequestError:
                print("Network error. Check your internet connection.")
            except Exception as e:
                print(f"Error: {e}")

if __name__ == "__main__":
    listen_for_commands()