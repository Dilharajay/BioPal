const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;
const AUTH_TOKEN = "Bearer your-token-here"; // Must match config.h API_AUTH_TOKEN

// Middleware to parse JSON
app.use(express.json());
// Serve static files (the beautiful dashboard)
app.use(express.static(path.join(__dirname, 'public')));

// Store connected SSE clients
let clients = [];
// Keep the last received data
let lastData = null;

// Endpoint for the ESP32 to POST data
app.post('/api/vitals', (req, res) => {
    const authHeader = req.headers['authorization'];
    if (authHeader !== AUTH_TOKEN) {
        console.warn('Unauthorized request attempt');
        return res.status(401).send('Unauthorized');
    }

    const data = req.body;
    lastData = data;
    console.log(`[DATA] Received Vitals: HR:${data.heartRate} T:${data.bodyTempC}C Fall:${data.fallDetected}`);

    // Broadcast data to all connected web clients
    clients.forEach(client => {
        client.res.write(`data: ${JSON.stringify(data)}\n\n`);
    });

    res.status(200).send('OK');
});

// Endpoint for frontend to subscribe to data stream (SSE)
app.get('/api/stream', (req, res) => {
    res.setHeader('Content-Type', 'text/event-stream');
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Connection', 'keep-alive');

    // Send the last data immediately if available
    if (lastData) {
        res.write(`data: ${JSON.stringify(lastData)}\n\n`);
    }

    const clientId = Date.now();
    const newClient = {
        id: clientId,
        res
    };
    clients.push(newClient);

    req.on('close', () => {
        clients = clients.filter(client => client.id !== clientId);
    });
});

app.listen(PORT, '0.0.0.0', () => {
    console.log(`Server running at http://0.0.0.0:${PORT}`);
    console.log(`Waiting for ESP32 data on POST /api/vitals...`);
    console.log(`Make sure to update config.h API_ENDPOINT with your PC's IP address!`);
});
