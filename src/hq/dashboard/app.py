from flask import Flask, render_template, request, jsonify, send_file
from flask_socketio import SocketIO, emit
import db
from serial import ArduinoSerial

app = Flask(__name__)
app.config['SECRET_KEY'] = 'lifi-mesh-hq-2025'
socketio = SocketIO(app, cors_allowed_origins="*")

# Arduino serial handler
arduino = None


# ==================== SERIAL MESSAGE CALLBACK ====================

def handle_arduino_message(data):
    """Called when Arduino receives a message from mesh"""
    sender_id = data['sender_id']
    msg_type = data['type']
    content = data['content']
    
    # Save to database
    msg_id = db.add_message(sender_id, msg_type, content)
    
    # Get full message data
    messages = db.get_messages(limit=1)
    if messages:
        message = messages[0]
        
        # Broadcast to all web clients
        socketio.emit('new_message', message)
        
        # Special SOS handling
        if msg_type == '3':
            node = db.get_node(sender_id)
            socketio.emit('sos_alert', {
                'node_id': sender_id,
                'node_name': node['name'] if node else sender_id,
                'content': content
            })
            print(f"🚨 SOS from {sender_id}: {content}")


# ==================== WEB ROUTES ====================

@app.route('/')
def index():
    """Serve dashboard HTML"""
    return send_file('dashboard.html')


@app.route('/api/nodes', methods=['GET'])
def get_nodes():
    """Get all nodes"""
    nodes = db.get_nodes()
    return jsonify(nodes)


@app.route('/api/nodes/<node_id>', methods=['GET'])
def get_node(node_id):
    """Get specific node"""
    node = db.get_node(node_id)
    if node:
        return jsonify(node)
    return jsonify({'error': 'Node not found'}), 404


@app.route('/api/nodes/<node_id>', methods=['PUT'])
def update_node(node_id):
    """Update node information"""
    data = request.json
    db.update_node(node_id, **data)
    node = db.get_node(node_id)
    return jsonify(node)


@app.route('/api/messages', methods=['GET'])
def get_messages():
    """Get recent messages"""
    limit = request.args.get('limit', 50, type=int)
    messages = db.get_messages(limit)
    return jsonify(messages)


@app.route('/api/stats', methods=['GET'])
def get_stats():
    """Get system statistics"""
    stats = db.get_stats()
    return jsonify(stats)


# ==================== WEBSOCKET EVENTS ====================

@socketio.on('connect')
def handle_connect():
    """Web client connected"""
    emit('status', {'connected': arduino.connected if arduino else False})
    print('✓ Web client connected')


@socketio.on('disconnect')
def handle_disconnect():
    """Web client disconnected"""
    print('✗ Web client disconnected')


@socketio.on('connect_arduino')
def handle_connect_arduino(data):
    """Connect to Arduino"""
    global arduino
    
    if arduino and arduino.connected:
        emit('arduino_status', {'status': 'already_connected'})
        return
    
    port = data.get('port', None)
    arduino = ArduinoSerial(on_message=handle_arduino_message)
    
    if arduino.connect(port):
        emit('arduino_status', {'status': 'connected'})
        socketio.emit('status', {'connected': True})
    else:
        emit('arduino_status', {'status': 'failed'})


@socketio.on('disconnect_arduino')
def handle_disconnect_arduino():
    """Disconnect from Arduino"""
    global arduino
    
    if arduino:
        arduino.disconnect()
        arduino = None
    
    emit('arduino_status', {'status': 'disconnected'})
    socketio.emit('status', {'connected': False})


@socketio.on('send_message')
def handle_send_message(data):
    """Send message through Arduino to mesh"""
    if not arduino or not arduino.connected:
        emit('send_result', {'success': False, 'error': 'Arduino not connected'})
        return
    
    destination = data.get('destination')
    msg_type = data.get('type')
    content = data.get('content')
    
    success = arduino.send(destination, msg_type, content)
    emit('send_result', {'success': success})


# ==================== MAIN ====================

if __name__ == '__main__':
    print("\n" + "=" * 60)
    print("🚨 LiFi Mesh Network - HQ Dashboard")
    print("=" * 60)
    print("\n📡 Starting server...")
    print("🌐 Open browser: http://localhost:5000")
    print("💡 Connect Arduino via USB after page loads")
    print("=" * 60 + "\n")
    
    try:
        socketio.run(app, debug=True, host='0.0.0.0', port=5000)
    except KeyboardInterrupt:
        print("\n\n👋 Shutting down...")
        if arduino:
            arduino.disconnect()
