// server.js — ESP32 Web Bridge
const express = require('express');
const http    = require('http');
const { WebSocketServer } = require('ws');
 
const app    = express();
const server = http.createServer(app);
const wss    = new WebSocketServer({ server });
 
let clients = [];
 
wss.on('connection', (ws) => {
  clients.push(ws);
  console.log('[WS] Browser connected. Clients: ' + clients.length);
  ws.on('close', () => {
    clients = clients.filter(c => c !== ws);
    console.log('[WS] Browser disconnected. Clients: ' + clients.length);
  });
});
 
// ESP32 (or browser test) calls this to trigger a click
app.get('/click', (req, res) => {
  console.log('[HTTP] /click received — broadcasting to ' + clients.length + ' client(s)');
  clients.forEach(ws => { if (ws.readyState === 1) ws.send('CLICK'); });
  res.send('OK');
});
 
// Simple status page so you can check the server is alive
app.get('/', (req, res) => {
  res.send('<h2>ESP32 Bridge Running</h2><p>Clients: ' + clients.length + '</p>');
});
 
server.listen(3000, '0.0.0.0', () => {
  console.log('');
  console.log('  Bridge server running!');
  console.log('  Local:   http://localhost:3000');
  console.log('  Status:  http://localhost:3000/');
  console.log('  Trigger: http://localhost:3000/click');
  console.log('');
});
