import serial
import serial.tools.list_ports
import threading
import time

class ArduinoSerial:
    """Handles serial communication with Arduino HQ"""
    
    def __init__(self, on_message=None):
        self.port = None
        self.serial = None
        self.connected = False
        self.on_message = on_message  # Callback function
        self.thread = None
        self.running = False
    
    def find_arduino(self):
        """Auto-detect Arduino/ESP port"""
        ports = serial.tools.list_ports.comports()
        
        # Priority 1: Look for Arduino/ESP specific identifiers
        for port in ports:
            desc = port.description.upper()
            # Common Arduino/ESP chips and drivers
            identifiers = [
                'ARDUINO',      # Arduino boards
                'CH340',        # Cheap Arduino clones, NodeMCU
                'CH341',        # Alternative CH340
                'CP210',        # ESP32, ESP8266 (Silicon Labs)
                'CP2102',       # Specific ESP32 chip
                'FT232',        # FTDI chips (some Arduinos)
                'USB-SERIAL',   # Generic USB serial
                'UART',         # USB UART bridges
                'USB2.0-SERIAL' # Some ESP boards
            ]
            if any(x in desc for x in identifiers):
                print(f"✓ Found device: {port.device} ({port.description})")
                return port.device
        
        # Priority 2: If nothing found, show all ports
        print("⚠️  No Arduino/ESP detected. Available ports:")
        for port in ports:
            print(f"   - {port.device}: {port.description}")
        
        return None
    
    def connect(self, port=None):
        """Connect to Arduino"""
        try:
            # Auto-detect if port not specified
            if port is None:
                port = self.find_arduino()
            
            if port is None:
                print("❌ No Arduino found")
                return False
            
            # Connect
            self.serial = serial.Serial(port, 115200, timeout=1)
            time.sleep(2)  # Wait for Arduino reset
            
            self.port = port
            self.connected = True
            
            # Start reading thread
            self.running = True
            self.thread = threading.Thread(target=self._read_loop, daemon=True)
            self.thread.start()
            
            print(f"✓ Connected to Arduino on {port}")
            return True
            
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from Arduino"""
        self.running = False
        
        if self.thread:
            self.thread.join(timeout=2)
        
        if self.serial and self.serial.is_open:
            self.serial.close()
        
        self.connected = False
        print("✓ Disconnected from Arduino")
    
    def _read_loop(self):
        """Background thread to read from serial"""
        while self.running:
            try:
                if self.serial and self.serial.in_waiting:
                    line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self._process_line(line)
                time.sleep(0.01)  # Small delay to prevent CPU spinning
            except Exception as e:
                print(f"❌ Read error: {e}")
                time.sleep(1)
    
    def _process_line(self, line):
        """Process received line from Arduino (V3 compatible)"""
        print(f"← {line}")
        
        # Handle status messages
        if line.startswith('READY') or line.startswith('INFO|'):
            return
        
        if line.startswith('OK|') or line.startswith('ERR|') or line.startswith('ERROR:'):
            return
        
        # Skip debug output from V3 firmware
        if (line.startswith('>>>') or line.startswith('╔') or 
            line.startswith('║') or line.startswith('╚') or 
            line.startswith('═') or line.startswith('─') or
            line.startswith('RX:') or line.startswith('TX:') or
            'COMPLETE PACKET' in line or 'INIT MESSAGE' in line or
            'SOS ALERT' in line or 'MESSAGE RECEIVED' in line):
            return
        
        # V3 Format: Parse clean output from HQ processPacket()
        # Format: <sender_id> <type> <content>
        # Examples:
        #   "102a 3 SOS"           (Type 3: SOS)
        #   "203b 4 Battery low"   (Type 4: MESSAGE)
        
        parts = line.split(' ', 2)
        
        if len(parts) >= 2:
            sender_id = parts[0]
            msg_type = parts[1]
            content = parts[2] if len(parts) > 2 else ''
            
            # Validate format (4-char sender, valid type)
            if len(sender_id) == 4 and msg_type in ['0', '1', '2', '3', '4']:
                # Call callback if provided
                if self.on_message:
                    self.on_message({
                        'sender_id': sender_id,
                        'type': msg_type,
                        'content': content if content else ('SOS' if msg_type == '3' else '')
                    })
                return
        
        # If we get here, it's unrecognized output (debug/status)
        # Just ignore it silently
    
    def send_init(self, init_id):
        """Send Type 0: INIT message to build gradient"""
        if not self.connected or not self.serial:
            print("❌ Not connected")
            return False
        
        try:
            command = f"INIT|{init_id}\n"
            self.serial.write(command.encode('utf-8'))
            print(f"→ INIT|{init_id}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
    
    def send_broadcast(self, message):
        """Send Type 1: Broadcast to all lamps"""
        if not self.connected or not self.serial:
            print("❌ Not connected")
            return False
        
        try:
            command = f"BROADCAST|{message}\n"
            self.serial.write(command.encode('utf-8'))
            print(f"→ BROADCAST|{message}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
    
    def send_targeted(self, node_id, message):
        """Send Type 2: Targeted message to specific lamp"""
        if not self.connected or not self.serial:
            print("❌ Not connected")
            return False
        
        try:
            command = f"TARGET|{node_id}|{message}\n"
            self.serial.write(command.encode('utf-8'))
            print(f"→ TARGET|{node_id}|{message}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
    
    def send_message(self, node_id, message):
        """Send Type 4: Normal message to node"""
        if not self.connected or not self.serial:
            print("❌ Not connected")
            return False
        
        try:
            command = f"MESSAGE|{node_id}|{message}\n"
            self.serial.write(command.encode('utf-8'))
            print(f"→ MESSAGE|{node_id}|{message}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False


# Test function
if __name__ == "__main__":
    def test_callback(data):
        print(f"📨 Received: {data}")
    
    arduino = ArduinoSerial(on_message=test_callback)
    
    if arduino.connect():
        print("\n🔧 Serial test mode")
        print("Commands:")
        print("  i <id>        - INIT (e.g., i 01)")
        print("  b <message>   - Broadcast")
        print("  t <nodeID> <msg>   - Target node")
        print("  m <nodeID> <msg>   - Send message")
        print("  q             - Quit\n")
        
        try:
            while True:
                cmd = input("> ").strip()
                
                if cmd == 'q':
                    break
                elif cmd.startswith('i '):
                    arduino.send_init(cmd[2:])
                elif cmd.startswith('b '):
                    arduino.send_broadcast(cmd[2:])
                elif cmd.startswith('t '):
                    parts = cmd.split(' ', 2)
                    if len(parts) == 3:
                        arduino.send_targeted(parts[1], parts[2])
                elif cmd.startswith('m '):
                    parts = cmd.split(' ', 2)
                    if len(parts) == 3:
                        arduino.send_message(parts[1], parts[2])
                        
        except KeyboardInterrupt:
            pass
        
        arduino.disconnect()