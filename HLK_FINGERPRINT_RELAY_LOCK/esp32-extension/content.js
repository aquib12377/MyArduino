// content.js — runs inside every browser tab
const SERVER = 'ws://localhost:3000';
let socket;
 
function connect() {
  socket = new WebSocket(SERVER);
 
  socket.onopen  = () => console.log('[Bridge] Connected to ESP32 server');
  socket.onclose = () => { console.log('[Bridge] Disconnected. Retrying...'); setTimeout(connect,3000); };
  socket.onerror = (e) => console.error('[Bridge] Error', e);
 
  socket.onmessage = (event) => {
    if (event.data === 'CLICK') {
      console.log('[Bridge] CLICK received!');
      clickButton();
    }
  };
}
 
function clickButton() {
  const selector = '#content div.aspect-1 > button';
  const btn = document.querySelector(selector);
  
  if (btn) {
    btn.click();
    console.log('[Bridge] Clicked ElevenLabs button ✅');
  } else {
    console.warn('[Bridge] Button not found — page may still be loading');
  }
}
 
connect();
