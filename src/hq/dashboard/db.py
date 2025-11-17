import sqlite3
from datetime import datetime
import os

DB_FILE = 'hq_data.db'

def init_database():
    """Initialize database with tables"""
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    
    # Create nodes table
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS nodes (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            latitude REAL,
            longitude REAL,
            status TEXT DEFAULT 'unknown',
            last_seen TIMESTAMP
        )
    ''')
    
    # Create messages table
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sender_id TEXT NOT NULL,
            type TEXT NOT NULL,
            content TEXT NOT NULL,
            is_sos BOOLEAN DEFAULT 0,
            timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (sender_id) REFERENCES nodes(id)
        )
    ''')
    
    # Add default HQ node if not exists
    cursor.execute("SELECT * FROM nodes WHERE id = '000h'")
    if not cursor.fetchone():
        cursor.execute('''
            INSERT INTO nodes (id, name, latitude, longitude, status, last_seen)
            VALUES ('000h', 'Headquarters', 22.5726, 88.3639, 'active', ?)
        ''', (datetime.now(),))
    
    conn.commit()
    conn.close()
    print("✓ Database initialized")


def add_node(node_id, name=None, lat=None, lon=None):
    """Add or update a node"""
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    
    if name is None:
        name = f"Node {node_id}"
    
    cursor.execute('''
        INSERT OR REPLACE INTO nodes (id, name, latitude, longitude, status, last_seen)
        VALUES (?, ?, ?, ?, 'active', ?)
    ''', (node_id, name, lat, lon, datetime.now()))
    
    conn.commit()
    conn.close()


def update_node(node_id, **kwargs):
    """Update node fields (name, lat, lon, status)"""
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    
    # Build dynamic UPDATE query
    fields = []
    values = []
    
    for key, value in kwargs.items():
        if key in ['name', 'latitude', 'longitude', 'status']:
            fields.append(f"{key} = ?")
            values.append(value)
    
    if fields:
        fields.append("last_seen = ?")
        values.append(datetime.now())
        values.append(node_id)
        
        query = f"UPDATE nodes SET {', '.join(fields)} WHERE id = ?"
        cursor.execute(query, values)
    
    conn.commit()
    conn.close()


def add_message(sender_id, msg_type, content):
    """Add a new message to database"""
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    
    is_sos = (msg_type == '3')
    
    # Ensure sender node exists
    cursor.execute("SELECT * FROM nodes WHERE id = ?", (sender_id,))
    if not cursor.fetchone():
        add_node(sender_id)
    
    # Update node status
    status = 'sos' if is_sos else 'active'
    cursor.execute('''
        UPDATE nodes SET status = ?, last_seen = ? WHERE id = ?
    ''', (status, datetime.now(), sender_id))
    
    # Insert message
    cursor.execute('''
        INSERT INTO messages (sender_id, type, content, is_sos, timestamp)
        VALUES (?, ?, ?, ?, ?)
    ''', (sender_id, msg_type, content, is_sos, datetime.now()))
    
    msg_id = cursor.lastrowid
    
    conn.commit()
    conn.close()
    
    return msg_id


def get_nodes():
    """Get all nodes"""
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row  # Return rows as dicts
    cursor = conn.cursor()
    
    cursor.execute("SELECT * FROM nodes")
    nodes = [dict(row) for row in cursor.fetchall()]
    
    conn.close()
    return nodes


def get_node(node_id):
    """Get a specific node"""
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    
    cursor.execute("SELECT * FROM nodes WHERE id = ?", (node_id,))
    node = cursor.fetchone()
    
    conn.close()
    return dict(node) if node else None


def get_messages(limit=50):
    """Get recent messages"""
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    
    cursor.execute('''
        SELECT m.*, n.name as sender_name
        FROM messages m
        LEFT JOIN nodes n ON m.sender_id = n.id
        ORDER BY m.timestamp DESC
        LIMIT ?
    ''', (limit,))
    
    messages = []
    for row in cursor.fetchall():
        msg = dict(row)
        # Add type name
        type_names = {'1': 'BROADCAST', '2': 'TARGETED', '3': 'SOS', '4': 'MESSAGE'}
        msg['type_name'] = type_names.get(msg['type'], 'UNKNOWN')
        messages.append(msg)
    
    conn.close()
    return messages


def get_stats():
    """Get system statistics"""
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    
    # Total nodes
    cursor.execute("SELECT COUNT(*) FROM nodes")
    total_nodes = cursor.fetchone()[0]
    
    # Active nodes
    cursor.execute("SELECT COUNT(*) FROM nodes WHERE status = 'active'")
    active_nodes = cursor.fetchone()[0]
    
    # SOS nodes
    cursor.execute("SELECT COUNT(*) FROM nodes WHERE status = 'sos'")
    sos_nodes = cursor.fetchone()[0]
    
    # Total messages
    cursor.execute("SELECT COUNT(*) FROM messages")
    total_messages = cursor.fetchone()[0]
    
    # SOS count
    cursor.execute("SELECT COUNT(*) FROM messages WHERE is_sos = 1")
    sos_count = cursor.fetchone()[0]
    
    conn.close()
    
    return {
        'total_nodes': total_nodes,
        'active_nodes': active_nodes,
        'sos_nodes': sos_nodes,
        'total_messages': total_messages,
        'sos_count': sos_count
    }


def clear_database():
    """Clear all data (for testing)"""
    if os.path.exists(DB_FILE):
        os.remove(DB_FILE)
        print("✓ Database cleared")
    init_database()


# Initialize database when module is imported
if not os.path.exists(DB_FILE):
    init_database()
