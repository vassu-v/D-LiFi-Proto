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
                print("Available ports:")
                for p in serial.tools.list_ports.comports():
                    print(f"  - {p.device}: {p.description}")
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
        """Process received line from Arduino"""
        print(f"← {line}")
        
        # Handle status messages
        if line.startswith('READY|') or line.startswith('INFO|'):
            return
        
        if line.startswith('OK|') or line.startswith('ERR|'):
            return
        
        # Skip debug output from V2.5/V3 firmware
        if line.startswith('>>>') or line.startswith('═') or line.startswith('─'):
            return
        
        # Parse V2.5 format: "RX IR: <header>"
        if 'RX IR:' in line or 'COMPLETE PACKET RECEIVED' in line:
            return  # Skip IR debug lines
        
        # Parse message format: <sender_id> <type> <content>
        # V2.5 header-only (Type 3 SOS): "102a000h3"
        # V2.5 with message (Type 1/2/4): "000hFFFF1ABCD" + "Evacuate"
        
        parts = line.split(' ', 2)
        
        # Type 3 (SOS) - header-only format
        if len(parts) == 1 and len(parts[0]) >= 9:
            header = parts[0]
            if len(header) == 9 and header[8] == '3':  # SOS
                sender_id = header[0:4]
                msg_type = '3'
                content = 'SOS'
                
                if self.on_message:
                    self.on_message({
                        'sender_id': sender_id,
                        'type': msg_type,
                        'content': content
                    })
                return
        
        # Standard format (Type 1/2/4)
        if len(parts) >= 2:
            sender_id = parts[0]
            msg_type = parts[1] if len(parts) > 1 else '4'
            content = parts[2] if len(parts) > 2 else ''
            
            # Validate format
            if len(sender_id) == 4 and msg_type in ['0', '1', '2', '3', '4']:
                # Call callback if provided
                if self.on_message:
                    self.on_message({
                        'sender_id': sender_id,
                        'type': msg_type,
                        'content': content
                    })
    
    def send(self, destination, msg_type, content):
        """
        Send message to Arduino
        Format: TX|<dst>|<type>|<message>
        """
        if not self.connected or not self.serial:
            print("❌ Not connected")
            return False
        
        try:
            command = f"TX|{destination}|{msg_type}|{content}\n"
            self.serial.write(command.encode('utf-8'))
            print(f"→ {command.strip()}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
    
    def send_init(self, init_id):
        """Send Type 0: INIT message to build gradient"""
        if not self.connected or not self.serial:
            print("❌ Not connected")
            return False
        
        try:
            command = f"INIT|{init_id}\n"
            self.serial.write(command.encode('utf-8'))
            print(f"→ INIT ID: {init_id}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
    
    def send_broadcast(self, message):
        """Send Type 1: Broadcast to all lamps"""
        return self.send("FFFF", "1", message, cmd_type="BROADCAST")
    
    def send_targeted(self, node_id, message):
        """Send Type 2: Targeted message to specific lamp"""
        return self.send(node_id, "2", message, cmd_type="TARGET")
    
    def send_message(self, node_id, message):
        """Send Type 4: Normal message to node"""
        return self.send(node_id, "4", message, cmd_type="MESSAGE")
    
    def send(self, destination, msg_type, content, cmd_type="MESSAGE"):
        """
        Send message to Arduino HQ (V3 format)
        
        Args:
            destination: Node ID or "FFFF" for broadcast
            msg_type: '1', '2', or '4'
            content: Message content
            cmd_type: Command prefix (BROADCAST, TARGET, MESSAGE)
        """
        if not self.connected or not self.serial:
            print("❌ Not connected")
            return False
        
        try:
            if cmd_type == "BROADCAST":
                command = f"BROADCAST|{content}\n"
            elif cmd_type == "TARGET":
                command = f"TARGET|{destination}|{content}\n"
            elif cmd_type == "MESSAGE":
                command = f"MESSAGE|{destination}|{content}\n"
            else:
                return False
            
            self.serial.write(command.encode('utf-8'))
            print(f"→ {command.strip()}")
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
        print("  b <message>        - Broadcast")
        print("  t <nodeID> <msg>   - Target node")
        print("  m <nodeID> <msg>   - Send message")
        print("  q                  - Quit\n")
        
        try:
            while True:
                cmd = input("> ").strip()
                
                if cmd == 'q':
                    break
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
