from flask import Flask, request, Response, send_from_directory
import queue
import json
import os

# Initialize Flask App
# We set static_folder to 'public' to serve the frontend files automatically.
app = Flask(__name__, static_folder='public')

PORT = int(os.environ.get('PORT', 3000))
AUTH_TOKEN = "Bearer your-token-here"

# Store connected SSE clients (each client gets a queue)
clients = []
# Keep the last received data
last_data = None

@app.route('/')
def serve_index():
    return send_from_directory('public', 'index.html')

@app.route('/<path:path>')
def serve_static(path):
    # This serves styles.css, app.js, etc.
    return send_from_directory('public', path)

@app.route('/api/vitals', methods=['POST'])
def receive_vitals():
    global last_data
    auth_header = request.headers.get('Authorization')
    
    if auth_header != AUTH_TOKEN:
        print('Unauthorized request attempt')
        return 'Unauthorized', 401

    data = request.json
    last_data = data
    print(f"[DATA] Received Vitals: HR:{data.get('heartRate', 0)} T:{data.get('bodyTempC', 0)}C Fall:{data.get('fallDetected', False)}")

    # Broadcast data to all connected web clients by placing it in their queue
    msg = json.dumps(data)
    for q in clients:
        q.put(msg)

    return 'OK', 200

@app.route('/api/stream')
def stream():
    def event_stream():
        # Create a queue for this specific client
        q = queue.Queue()
        clients.append(q)
        
        try:
            # Send the last data immediately if available
            if last_data:
                yield f"data: {json.dumps(last_data)}\n\n"
                
            # Block and wait for new messages
            while True:
                msg = q.get()
                yield f"data: {msg}\n\n"
        except GeneratorExit:
            # Client disconnected
            if q in clients:
                clients.remove(q)
                
    return Response(event_stream(), mimetype='text/event-stream', headers={
        'Cache-Control': 'no-cache',
        'Connection': 'keep-alive'
    })

if __name__ == '__main__':
    print(f"Server running at http://0.0.0.0:{PORT}")
    print("Waiting for ESP32 data on POST /api/vitals...")
    print("Make sure to update the device via CLI (set api http://<YOUR_PC_IP>:3000/api/vitals your-token-here)!")
    # threaded=True is required to handle concurrent SSE connections
    app.run(host='0.0.0.0', port=PORT, threaded=True)
