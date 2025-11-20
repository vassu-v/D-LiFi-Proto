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
        self.on_message = on_message
        self.thread = None
        self.running = False
    
    def find_arduino(self):
        """Auto-detect Arduino/ESP port"""
        ports = serial.tools.list_ports.comports()
        
        for port in ports:
            desc = port.description.upper()
            identifiers = ['ARDUINO', 'CH340', 'CH341', 'CP210', 'CP2102', 
                          'FT232', 'USB-SERIAL', 'UART', 'USB2.0-SERIAL']
            if any(x in desc for x in identifiers):
                print(f"✓ Found: {port.device} ({port.description})")
                return port.device
        
        print("⚠️  No Arduino detected. Available ports:")
        for port in ports:
            print(f"   - {port.device}: {port.description}")
        return None
    
    def connect(self, port=None):
        """Connect to Arduino"""
        try:
            if port is None:
                port = self.find_arduino()
            
            if port is None:
                print("❌ No Arduino found")
                return False
            
            self.serial = serial.Serial(port, 115200, timeout=1)
            time.sleep(2)
            
            self.port = port
            self.connected = True
            
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
        print("✓ Disconnected")
    
    def _read_loop(self):
        """Background thread to read from serial"""
        while self.running:
            try:
                if self.serial and self.serial.in_waiting:
                    line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self._process_line(line)
                time.sleep(0.01)
            except Exception as e:
                print(f"❌ Read error: {e}")
                time.sleep(1)
    
    def _process_line(self, line):
        """Process received line from Arduino (V3 compatible)"""
        print(f"← {line}")
        
        # Skip status and debug messages
        if (line.startswith(('READY', 'INFO|', 'OK|', 'ERR|', 'ERROR:', '>>>', 
                            '╔', '║', '╚', '═', '─', 'RX:', 'TX:')) or
            any(x in line for x in ['COMPLETE PACKET', 'INIT MESSAGE', 
                                    'SOS ALERT', 'MESSAGE RECEIVED'])):
            return
        
        # Parse: <sender_id> <type> <content>
        parts = line.split(' ', 2)
        
        if len(parts) >= 2:
            sender_id = parts[0]
            msg_type = parts[1]
            content = parts[2] if len(parts) > 2 else ''
            
            if len(sender_id) == 4 and msg_type in ['0', '1', '2', '3', '4']:
                if self.on_message:
                    self.on_message({
                        'sender_id': sender_id,
                        'type': msg_type,
                        'content': content if content else ('SOS' if msg_type == '3' else '')
                    })
    
    def send_init(self, init_id):
        """Send Type 0: INIT"""
        if not self.connected or not self.serial:
            return False
        try:
            self.serial.write(f"INIT|{init_id}\n".encode('utf-8'))
            print(f"→ INIT|{init_id}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
    
    def send_broadcast(self, message):
        """Send Type 1: Broadcast"""
        if not self.connected or not self.serial:
            return False
        try:
            self.serial.write(f"BROADCAST|{message}\n".encode('utf-8'))
            print(f"→ BROADCAST|{message}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
    
    def send_targeted(self, node_id, message):
        """Send Type 2: Targeted"""
        if not self.connected or not self.serial:
            return False
        try:
            self.serial.write(f"TARGET|{node_id}|{message}\n".encode('utf-8'))
            print(f"→ TARGET|{node_id}|{message}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
    
    def send_message(self, node_id, message):
        """Send Type 4: Message"""
        if not self.connected or not self.serial:
            return False
        try:
            self.serial.write(f"MESSAGE|{node_id}|{message}\n".encode('utf-8'))
            print(f"→ MESSAGE|{node_id}|{message}")
            return True
        except Exception as e:
            print(f"❌ Send failed: {e}")
            return False
