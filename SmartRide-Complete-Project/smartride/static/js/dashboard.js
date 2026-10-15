let map,marker,countdownTimer;
function initMap(){if(typeof L==='undefined')return;map=L.map('map',{zoomControl:true}).setView([19.076,72.8777],13);L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:19,attribution:'© OpenStreetMap'}).addTo(map)}
function text(id,value){const el=document.getElementById(id);if(el)el.textContent=value}
function eventHtml(e){return `<div class="event ${e.severity||'info'}"><i></i><div><b>${escapeHtml(e.message)}</b><small>${new Date(e.created_at).toLocaleString()}</small></div></div>`}
function escapeHtml(v){const d=document.createElement('div');d.textContent=v||'';return d.innerHTML}
function startCountdown(deadline){clearInterval(countdownTimer);const banner=document.getElementById('accidentBanner');if(!deadline){banner?.classList.add('hidden');return}banner?.classList.remove('hidden');const tick=()=>{const left=Math.max(0,new Date(deadline)-Date.now());text('countdown',`${String(Math.floor(left/60000)).padStart(2,'0')}:${String(Math.floor((left%60000)/1000)).padStart(2,'0')}`)};tick();countdownTimer=setInterval(tick,500)}
function updateState(s){
 text('connectionText',s.helmet_online?'Helmet online':'Helmet offline');
 text('helmetStatus',s.helmet_worn?'Worn':'Not worn');text('helmetDetail',s.helmet_online?'Sensor reporting':'Telemetry unavailable');
 text('alcoholStatus',s.alcohol_detected?'Detected':'Not detected');text('alcoholDetail',s.alcohol_detected?'Ignition locked':'Safe to start');
 text('rideStatus',s.bike_running?'Ride active':(s.ignition_allowed?'Ready':'Stopped'));text('rideDetail',s.ignition_allowed?'Ignition permitted':'Ignition locked');
 const battery=Math.max(0,Math.min(100,Number(s.battery||0)));text('batteryStatus',`${battery.toFixed(0)}%`);const bar=document.getElementById('batteryBar');if(bar)bar.style.width=`${battery}%`;
 if(s.latitude!=null&&s.longitude!=null){text('coordinates',`${Number(s.latitude).toFixed(5)}, ${Number(s.longitude).toFixed(5)}`);if(map){const ll=[s.latitude,s.longitude];if(!marker)marker=L.marker(ll).addTo(map).bindPopup('Rider');else marker.setLatLng(ll);map.setView(ll,16)}}
 const pending=s.accident_status==='pending';startCountdown(pending?s.accident_deadline:null);
 const confirmed=s.accident_status==='confirmed';text('emergencyTitle',confirmed?'Emergency confirmed':pending?'Checking rider':'All clear');text('emergencyText',confirmed?'Emergency alert confirmed. Contact the rider now.':pending?'Waiting for rider cancellation.':'No emergency alerts reported.');text('smsStatus',s.last_sms_status|| (confirmed?'Triggered':'Not required'));
 const score=Number(s.live_score??100);text('scoreValue',score);const ring=document.getElementById('scoreRing');if(ring)ring.style.setProperty('--score',score);text('scoreLabel',score>=90?'Excellent ride':score>=75?'Good ride':score>=50?'Needs improvement':'High risk');text('scoreExplanation',s.score_explanation||'Score updates during the ride.');
 if(Array.isArray(s.recent_events)){const events=document.getElementById('events');if(events)events.innerHTML=s.recent_events.length?s.recent_events.map(eventHtml).join(''):'<p class="empty">No events yet.</p>'}
}
async function refresh(){try{const r=await fetch('/api/state');if(r.ok)updateState(await r.json())}catch(e){text('connectionText','Server reconnecting…')}}
initMap();refresh();setInterval(refresh,2000);if(typeof io!=='undefined'){const socket=io();socket.on('state_update',refresh)}

