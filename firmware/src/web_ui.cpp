#include "web_ui.h"
#include "config.h"
#include "net.h"
#include "thermal.h"
#include "key_matrix.h"
#include "sleep_debt.h"
#include "buzzer.h"
#include "display.h"
#include "mic.h"
#include "sleep_model.h"
#include "alarm.h"
#include "settings.h"

WebUI webui;

static const char PAGE_MAIN[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>inSomnia</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;background:#0b0d10;color:#e6e9ef;
 font:15px/1.5 ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:600px;margin:0 auto;padding:20px 16px 56px}
h1{font-size:13px;letter-spacing:.18em;text-transform:uppercase;
 margin:0 0 18px;color:#79839a;font-weight:600}
.clock{font-size:62px;font-weight:200;letter-spacing:-.02em;line-height:1;
 font-variant-numeric:tabular-nums;margin:0 0 4px}
.date{color:#79839a;font-size:13px;margin-bottom:8px}
.state{display:inline-block;padding:4px 12px;border-radius:99px;font-size:13px;
 font-weight:600;letter-spacing:.03em;margin-bottom:22px}
.s-asleep{background:#0f3527;color:#4ade80}
.s-stirring{background:#3a3010;color:#facc15}
.s-awake{background:#3a1f10;color:#fb923c}
.s-unknown{background:#1c2129;color:#79839a}
.card{background:#14181f;border:1px solid #232937;border-radius:12px;
 padding:14px 16px;margin-bottom:12px}
.card h2{font-size:11px;letter-spacing:.14em;text-transform:uppercase;
 color:#79839a;margin:0 0 12px;font-weight:600}
.row{display:flex;justify-content:space-between;align-items:center;
 padding:6px 0;border-bottom:1px solid #1c2129;font-size:14px;gap:12px}
.row:last-child{border-bottom:0}
.row>span:first-child{color:#79839a;flex:0 0 auto}
.v{font-variant-numeric:tabular-nums;text-align:right}
.big{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:4px}
.stat{background:#0f1319;border:1px solid #1c2129;border-radius:9px;padding:10px 8px;
 text-align:center}
.stat b{display:block;font-size:19px;font-weight:600;font-variant-numeric:tabular-nums}
.stat span{font-size:10px;color:#5b6479;letter-spacing:.06em;text-transform:uppercase}
.bar{height:8px;background:#1c2129;border-radius:4px;overflow:hidden;margin-top:9px}
.bar>i{display:block;height:100%;background:#4c8dff;width:0;transition:width .4s}
button{background:#232937;color:#e6e9ef;border:1px solid #2e3648;
 border-radius:8px;padding:9px 14px;font-size:14px;cursor:pointer;font-weight:500}
button:hover{background:#2b3244}
button.pri{background:#2563eb;border-color:#2563eb;color:#fff}
button.dng{background:#b91c1c;border-color:#b91c1c;color:#fff}
.btns{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}
input,select{background:#0b0d10;border:1px solid #2e3648;border-radius:8px;
 padding:8px 10px;color:#e6e9ef;font-size:14px;width:100%}
input[type=range]{padding:0}
label{display:block;font-size:12px;color:#79839a;margin:10px 0 4px}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.note{color:#5b6479;font-size:12px;margin-top:12px;line-height:1.55}
.warn{background:#3a1f10;border:1px solid #7c2d12;color:#fdba74;padding:10px 12px;
 border-radius:9px;font-size:13px;margin-bottom:12px;display:none}
</style></head><body><div class="wrap">
<h1>inSomnia</h1>
<div class="clock" id="clock">--:--:--</div>
<div class="date" id="date">waiting for time sync</div>
<div class="state s-unknown" id="state">unknown</div>

<div class="warn" id="failsafe"></div>

<div class="card"><h2>Tonight</h2>
<div class="row"><span>Alarm</span><span class="v" id="phase">-</span></div>
<div class="row"><span>Window</span><span class="v" id="window">-</span></div>
<div class="row"><span>Counts this epoch</span><span class="v" id="counts">-</span></div>
<div class="row"><span>Out of bed</span><span class="v" id="oob">-</span></div>
<div class="btns">
<button class="dng" onclick="al('dismiss')">Dismiss</button>
<button onclick="al('snooze')">Snooze</button>
<button onclick="al('test-gentle')">Test gentle</button>
<button onclick="al('test-hard')">Test hard</button>
<button onclick="al('stop')">Stop test</button>
</div>
</div>

<div class="card"><h2>Last night</h2>
<div class="big">
<div class="stat"><b id="tst">-</b><span>Sleep</span></div>
<div class="stat"><b id="eff">-</b><span>Efficiency</span></div>
<div class="stat"><b id="sol">-</b><span>Onset</span></div>
<div class="stat"><b id="waso">-</b><span>Awake</span></div>
</div>
<div class="row"><span>Outcome</span><span class="v" id="outcome">-</span></div>
<div class="note"><b>Read these as estimates, not measurements.</b> They come from
Cole-Kripke scoring of mattress motion, which agrees with clinical
polysomnography about 78-80% of the time for sleep vs wake &mdash; and only
0.35-0.64 specificity on <i>wake</i>, so awake time is systematically
under-reported. There is no EEG here, so there is no true sleep staging.</div>
</div>

<div class="card"><h2>Sleep debt</h2>
<div class="big">
<div class="stat"><b id="dcum">-</b><span>Debt 14d</span></div>
<div class="stat"><b id="drec">-</b><span>Recent</span></div>
<div class="stat"><b id="davg">-</b><span>Avg night</span></div>
<div class="stat"><b id="dshift">-</b><span>Wake delay</span></div>
</div>
<div class="row"><span>Nights recorded</span><span class="v" id="dn">-</span></div>
<div class="note">Debt is <code>max(0, need &minus; actual)</code> summed over a
rolling 14 nights, which is the window research finds matters for current
impairment; "Recent" weights the last few nights more heavily. When you're
carrying debt the gentle wake is held back by up to the shift shown &mdash;
<b>the hard deadline never moves.</b> These are estimates built on an estimate:
actual sleep comes from Cole-Kripke scoring, which under-reports wake.</div>
</div>

<div class="card"><h2>Thermal view</h2>
<canvas id="th" width="320" height="240"
 style="width:100%;image-rendering:pixelated;border-radius:8px;background:#0f1319"></canvas>
<div class="row" style="margin-top:8px"><span>In bed</span><span class="v" id="inbed">-</span></div>
<div class="row"><span>Warm pixels</span><span class="v" id="warm">-</span></div>
<div class="row"><span>Motion index</span><span class="v" id="motion">-</span></div>
<div class="row"><span>Peak / ambient</span><span class="v" id="temps">-</span></div>
<div class="row"><span>Epoch counts</span><span class="v" id="thc">-</span></div>
<div class="note">32&times;24 far-infrared, about 1000&times; coarser than a
photo &mdash; you cannot identify a person from it. Frame-to-frame change is
the contactless equivalent of an actigraphy count; video actigraphy validates
against polysomnography at Cohen's &kappa; 0.733. It reports <i>movement and
position</i>, not sleeping posture &mdash; posture under a duvet at this
resolution is a research problem, not a solved one.</div>
</div>

<div class="card"><h2>Microphone</h2>
<div class="row"><span>RMS / dBFS</span><span class="v" id="mrms">-</span></div>
<div class="row"><span>Loud fraction</span><span class="v" id="mloud">-</span></div>
<div class="note">A level detector, not a sound classifier &mdash; it reports
how loud the room is, not what it heard.</div>
</div>

<div class="card"><h2>Settings</h2>
<div class="grid2">
<div><label>Window opens</label><input type="time" id="wopen"></div>
<div><label>Hard deadline</label><input type="time" id="wdead"></div>
</div>
<div class="grid2">
<div><label>Stirring counts</label><input type="number" id="stir" min="1" max="65535"></div>
<div><label>Awake counts</label><input type="number" id="awake" min="1" max="65535"></div>
</div>
<div class="grid2">
<div><label>Count scale (calibration)</label><input type="number" id="scale" min="1" max="65535"></div>
<div><label>Confirm epochs</label><input type="number" id="confirm" min="1" max="20"></div>
</div>
<div class="grid2">
<div><label>Gentle start volume</label><input type="number" id="gvol" min="0" max="255"></div>
<div><label>Ramp minutes</label><input type="number" id="gramp" min="1" max="60"></div>
</div>
<div class="grid2">
<div><label>Snooze minutes</label><input type="number" id="snz" min="1" max="60"></div>
<div><label>Backlight</label><input type="number" id="bl" min="0" max="255"></div>
</div>
<div class="grid2">
<div><label>Sleep need (min/night)</label><input type="number" id="need" min="240" max="720"></div>
<div><label>Max debt wake delay (min)</label><input type="number" id="shift" min="0" max="60"></div>
</div>
<div class="grid2">
<div><label>Body margin (0.1&deg;C over ambient)</label><input type="number" id="bodym" min="5" max="150"></div>
<div><label>In-bed min warm pixels</label><input type="number" id="inbedpx" min="1" max="768"></div>
</div>
<label><input type="checkbox" id="dae" style="width:auto"> Sleep-debt-aware waking</label>
<label><input type="checkbox" id="aen" style="width:auto"> Alarm enabled</label>
<label><input type="checkbox" id="sen" style="width:auto"> Suppress if already up</label>
<div class="btns">
<button class="pri" onclick="saveCfg()">Save settings</button>
<button onclick="defCfg()">Restore defaults</button>
</div>
<div class="note"><b>Count scale</b> is the one number you must calibrate.
Cole-Kripke's weights assume ActiGraph counts; ours are arbitrary units from
your own mattress, so this divides ours into that range. Watch the epoch counts
overnight and set it so a quiet epoch lands near zero.</div>
</div>

<div class="card"><h2>WiFi</h2>
<div class="row"><span>Status</span><span class="v" id="net">-</span></div>
<div class="row"><span>Address</span><span class="v" id="ip">-</span></div>
<input id="s" placeholder="SSID" autocomplete="off" style="margin-top:10px">
<input id="p" placeholder="Password" type="password" autocomplete="off" style="margin-top:8px">
<div class="btns">
<button class="pri" onclick="savewifi()">Save &amp; reboot</button>
<button onclick="forget()">Forget network</button>
</div>
<div class="note">Everything runs on your own network. There is no cloud service
and no audio ever leaves the device.</div>
</div>
</div>
<script>
function $(i){return document.getElementById(i);}
function post(u,b){return fetch(u,{method:'POST',
 headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});}
function al(a){post('/api/alarm','action='+a);}
function hhmm(m){var h=Math.floor(m/60),n=m%60;
 return (h<10?'0':'')+h+':'+(n<10?'0':'')+n;}
function mins(v){var p=(v||'00:00').split(':');
 return parseInt(p[0],10)*60+parseInt(p[1],10);}
function savewifi(){var s=$('s').value;if(!s){alert('SSID required');return;}
 post('/api/wifi','ssid='+encodeURIComponent(s)+'&pass='+
  encodeURIComponent($('p').value)).then(function(){alert('Saved. Rebooting.');});}
function forget(){if(confirm('Forget the stored network?'))
 post('/api/forget','').then(function(){alert('Cleared. Rebooting.');});}
function defCfg(){if(confirm('Restore default settings?'))
 post('/api/settings','defaults=1').then(loadCfg);}
function saveCfg(){
 var q='wopen='+mins($('wopen').value)+'&wdead='+mins($('wdead').value)
  +'&stir='+$('stir').value+'&awake='+$('awake').value
  +'&scale='+$('scale').value+'&confirm='+$('confirm').value
  +'&gvol='+$('gvol').value+'&gramp='+$('gramp').value
  +'&snz='+$('snz').value+'&bl='+$('bl').value
  +'&need='+$('need').value+'&shift='+$('shift').value
  +'&bodym='+$('bodym').value+'&inbedpx='+$('inbedpx').value
  +'&aen='+($('aen').checked?1:0)+'&sen='+($('sen').checked?1:0)
  +'&dae='+($('dae').checked?1:0);
 post('/api/settings',q).then(function(){loadCfg();});}
function loadCfg(){fetch('/api/settings').then(function(r){return r.json();})
.then(function(c){
 $('wopen').value=hhmm(c.wopen);$('wdead').value=hhmm(c.wdead);
 $('stir').value=c.stir;$('awake').value=c.awake;$('scale').value=c.scale;
 $('confirm').value=c.confirm;$('gvol').value=c.gvol;$('gramp').value=c.gramp;
 $('snz').value=c.snz;$('bl').value=c.bl;
 $('need').value=c.need;$('shift').value=c.shift;
 $('bodym').value=c.bodym;$('inbedpx').value=c.inbedpx;
 $('aen').checked=c.aen;$('sen').checked=c.sen;$('dae').checked=c.dae;});}
function tick(){fetch('/api/state').then(function(r){return r.json();})
.then(function(d){
 $('clock').textContent=d.time;
 $('date').textContent=d.synced?d.date:'waiting for time sync';
 var st=$('state');st.textContent=d.state;st.className='state s-'+d.state;
 var f=$('failsafe');
 if(d.failsafe){f.style.display='block';
  f.textContent='Failsafe active - sensors unavailable, so stirring detection '+
  'and suppression are off. A plain fixed-time alarm will fire at the deadline.';}
 else f.style.display='none';
 $('phase').textContent=d.phase;
 $('window').textContent=d.toOpen>0?('opens in '+d.toOpen+' min')
  :('deadline in '+d.toDeadline+' min');
 $('counts').textContent=d.counts;
 $('oob').textContent=d.oob?'yes':'no';
 $('tst').textContent=d.n_tst?(Math.floor(d.n_tst/60)+'h '+(d.n_tst%60)+'m'):'-';
 $('eff').textContent=d.n_eff?(d.n_eff+'%'):'-';
 $('sol').textContent=d.n_sol!==undefined?(d.n_sol+'m'):'-';
 $('waso').textContent=d.n_waso!==undefined?(d.n_waso+'m'):'-';
 $('outcome').textContent=d.outcome;
 $('mrms').textContent=d.mic?(d.rms+' / '+d.dbfs.toFixed(1)+' dB'):'absent';
 $('mloud').textContent=(d.loud*100).toFixed(0)+'%';
 $('inbed').textContent=d.th?(d.inbed?'yes':'no'):'sensor offline';
 $('warm').textContent=d.warm;
 $('motion').textContent=d.motion.toFixed(3);
 $('temps').textContent=d.peakC.toFixed(1)+' / '+d.ambC.toFixed(1)+' \u00b0C';
 $('thc').textContent=d.thCounts;
 $('dcum').textContent=hm(d.debtCum);
 $('drec').textContent=hm(d.debtRecent);
 $('davg').textContent=hm(d.debtAvg);
 $('dshift').textContent=d.debtShift+'m';
 $('dn').textContent=d.debtNights+' / 14';
 $('net').textContent=d.net;$('ip').textContent=d.ip;
}).catch(function(){});}
function hm(m){if(m===undefined||m===null)return '-';
 var s=m<0?'-':'';m=Math.abs(m);
 return s+Math.floor(m/60)+'h '+(m%60)+'m';}
var ctx=$('th').getContext('2d');
function drawTh(){fetch('/api/thermal').then(function(r){return r.json();})
.then(function(t){
 var d=t.d,lo=1e9,hi=-1e9,i;
 for(i=0;i<d.length;i++){if(d[i]<lo)lo=d[i];if(d[i]>hi)hi=d[i];}
 var rng=Math.max(1,hi-lo);
 var img=ctx.createImageData(t.w,t.h);
 for(i=0;i<d.length;i++){
  var v=(d[i]-lo)/rng;
  // simple inferno-ish ramp: dark blue -> magenta -> orange -> white
  var r=Math.min(255,Math.round(255*Math.pow(v,0.7)));
  var g=Math.min(255,Math.round(255*Math.pow(Math.max(0,v-0.35)/0.65,1.4)));
  var b=Math.round(255*Math.max(0,Math.min(1,1.6*v*(1-v)*2.2)));
  img.data[i*4]=r;img.data[i*4+1]=g;img.data[i*4+2]=b;img.data[i*4+3]=255;}
 var off=document.createElement('canvas');off.width=t.w;off.height=t.h;
 off.getContext('2d').putImageData(img,0,0);
 ctx.imageSmoothingEnabled=false;
 ctx.clearRect(0,0,320,240);ctx.drawImage(off,0,0,320,240);
 if(t.inbed){ctx.strokeStyle='#4ade80';ctx.lineWidth=2;
  ctx.beginPath();ctx.arc(t.cx*10+5,t.cy*10+5,9,0,6.28);ctx.stroke();}
}).catch(function(){});}
setInterval(tick,1000);tick();loadCfg();
setInterval(drawTh,1500);drawTh();
</script></body></html>)HTML";

static const char PAGE_SETUP[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>inSomnia setup</title>
<style>
:root{color-scheme:dark}
body{margin:0;background:#0b0d10;color:#e6e9ef;
 font:15px/1.6 ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
 display:flex;min-height:100vh;align-items:center;justify-content:center;padding:20px}
.box{width:100%;max-width:380px}
h1{font-size:13px;letter-spacing:.18em;text-transform:uppercase;color:#79839a;margin:0 0 6px}
p{color:#79839a;font-size:13px;margin:0 0 20px}
input{background:#14181f;border:1px solid #2e3648;border-radius:8px;
 padding:11px 13px;color:#e6e9ef;font-size:15px;width:100%;margin-bottom:10px;
 box-sizing:border-box}
button{background:#2563eb;color:#fff;border:0;border-radius:8px;padding:12px;
 font-size:15px;width:100%;cursor:pointer;font-weight:600}
ul{list-style:none;padding:0;margin:14px 0 0;font-size:13px}
li{padding:9px 12px;background:#14181f;border:1px solid #232937;border-radius:8px;
 margin-bottom:6px;cursor:pointer;display:flex;justify-content:space-between}
li:hover{background:#1c2129}
li span{color:#5b6479}
</style></head><body><div class="box">
<h1>inSomnia</h1>
<p>Join this device to your WiFi. It will reboot and then be reachable at
<b>insomnia.local</b>.</p>
<input id="s" placeholder="SSID" autocomplete="off">
<input id="p" placeholder="Password" type="password" autocomplete="off">
<button onclick="save()">Connect</button>
<ul id="list"><li>Scanning&hellip;</li></ul>
</div>
<script>
fetch('/api/scan').then(function(r){return r.json();}).then(function(d){
 var l=document.getElementById('list');l.innerHTML='';
 if(!d.nets.length){l.innerHTML='<li>No networks found</li>';return;}
 d.nets.forEach(function(n){
  var li=document.createElement('li');
  li.innerHTML='<b></b><span>'+n.rssi+' dBm</span>';
  li.querySelector('b').textContent=n.ssid;
  li.onclick=function(){document.getElementById('s').value=n.ssid;
   document.getElementById('p').focus();};
  l.appendChild(li);});
}).catch(function(){document.getElementById('list').innerHTML='<li>Scan failed</li>';});
function save(){var s=document.getElementById('s').value;
 if(!s){alert('SSID required');return;}
 fetch('/api/wifi',{method:'POST',
  headers:{'Content-Type':'application/x-www-form-urlencoded'},
  body:'ssid='+encodeURIComponent(s)+'&pass='+
   encodeURIComponent(document.getElementById('p').value)})
 .then(function(){document.body.innerHTML=
  '<div class="box"><h1>inSomnia</h1><p>Saved. Rebooting &mdash; reconnect your '+
  'phone to your normal WiFi, then open <b>http://insomnia.local</b></p></div>';});}
</script></body></html>)HTML";

WebUI::WebUI() : _server(80), _rebootPending(false), _rebootAtMs(0) {}

void WebUI::begin() {
    routes();
    _server.begin();
    Serial.println("[web] server listening on :80");
}

void WebUI::routes() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/state", HTTP_GET, [this]() { handleState(); });
    _server.on("/api/thermal", HTTP_GET, [this]() { handleThermal(); });
    _server.on("/api/settings", HTTP_GET, [this]() { handleGetSettings(); });
    _server.on("/api/settings", HTTP_POST, [this]() { handleSetSettings(); });
    _server.on("/api/alarm", HTTP_POST, [this]() { handleAlarm(); });
    _server.on("/api/scan", HTTP_GET, [this]() { handleScan(); });
    _server.on("/api/wifi", HTTP_POST, [this]() { handleWifiSave(); });
    _server.on("/api/forget", HTTP_POST, [this]() { handleForget(); });
    _server.on("/api/buzzer", HTTP_POST, [this]() { handleBuzzer(); });
    _server.on("/api/backlight", HTTP_POST, [this]() { handleBacklight(); });
    _server.onNotFound([this]() {
        _server.sendHeader("Location", "/", true);
        _server.send(302, "text/plain", "");
    });
}

void WebUI::loop() {
    _server.handleClient();
    if (_rebootPending && millis() > _rebootAtMs) ESP.restart();
}

void WebUI::handleRoot() {
    if (net.isSetupMode()) _server.send_P(200, "text/html", PAGE_SETUP);
    else                   _server.send_P(200, "text/html", PAGE_MAIN);
}

void WebUI::handleState() {
    struct tm t;
    bool trusted = false;
    bool hasTime = net.timeForAlarm(&t, &trusted);
    char timeBuf[16] = "--:--:--";
    char dateBuf[40] = "";
    if (hasTime) {
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &t);
        if (!trusted) strncat(timeBuf, "*", sizeof(timeBuf) - strlen(timeBuf) - 1);
        strftime(dateBuf, sizeof(dateBuf), "%A, %d %B %Y", &t);
    }
    bool synced = trusted;

    const NightRecord& n = alarmEngine.lastNight();

    String j = "{";
    j += "\"time\":\"" + String(timeBuf) + "\",";
    j += "\"date\":\"" + String(dateBuf) + "\",";
    j += "\"synced\":" + String(synced ? "true" : "false") + ",";
    j += "\"state\":\"" + String(sleepModel.stateName()) + "\",";
    j += "\"counts\":" + String(sleepModel.lastCounts()) + ",";
    j += "\"oob\":" + String(sleepModel.outOfBed() ? "true" : "false") + ",";
    j += "\"phase\":\"" + String(alarmEngine.phaseName()) + "\",";
    j += "\"outcome\":\"" + String(AlarmEngine::outcomeNameOf(n.outcome)) + "\",";
    j += "\"failsafe\":" + String(alarmEngine.failsafeActive() ? "true" : "false") + ",";
    j += "\"toOpen\":" + String(alarmEngine.minutesToWindowOpen()) + ",";
    j += "\"toDeadline\":" + String(alarmEngine.minutesToDeadline()) + ",";
    j += "\"n_tst\":" + String(n.totalSleepMin) + ",";
    j += "\"n_eff\":" + String(n.sleepEfficiencyPct) + ",";
    j += "\"n_sol\":" + String(n.solMin) + ",";
    j += "\"n_waso\":" + String(n.wasoMin) + ",";
    j += "\"th\":" + String(thermal.healthy() ? "true" : "false") + ",";
    j += "\"inbed\":" + String(thermal.inBed() ? "true" : "false") + ",";
    j += "\"warm\":" + String(thermal.warmPixels()) + ",";
    j += "\"motion\":" + String(thermal.motionIndex(), 4) + ",";
    j += "\"peakC\":" + String(thermal.peakTempC(), 1) + ",";
    j += "\"ambC\":" + String(thermal.ambientTempC(), 1) + ",";
    j += "\"thCounts\":" + String(thermal.epochCounts()) + ",";
    j += "\"debtCum\":" + String(sleepDebt.cumulativeDebtMin()) + ",";
    j += "\"debtRecent\":" + String(sleepDebt.recentDebtMin()) + ",";
    j += "\"debtNights\":" + String(sleepDebt.nightsRecorded()) + ",";
    j += "\"debtAvg\":" + String(sleepDebt.averageSleepMin()) + ",";
    j += "\"debtShift\":" + String(alarmEngine.debtShiftMinutes()) + ",";
    j += "\"mic\":" + String(mic.healthy() ? "true" : "false") + ",";
    j += "\"rms\":" + String(mic.rms()) + ",";
    j += "\"dbfs\":" + String(mic.dbfs(), 1) + ",";
    j += "\"loud\":" + String(mic.loudFraction(), 3) + ",";
    j += "\"keys\":" + String(keys.state()) + ",";
    j += "\"net\":\"" + String(net.modeName()) + "\",";
    j += "\"ip\":\"" + net.ipAddress() + "\",";
    j += "\"heap\":" + String(ESP.getFreeHeap());
    j += "}";

    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", j);
}

void WebUI::handleThermal() {
    const float* f = thermal.frame();
    String j;
    j.reserve(4600);
    j = "{\"w\":32,\"h\":24,\"inbed\":";
    j += thermal.inBed() ? "true" : "false";
    j += ",\"cx\":" + String(thermal.centroidX(), 1);
    j += ",\"cy\":" + String(thermal.centroidY(), 1);
    j += ",\"d\":[";
    for (int i = 0; i < TH_PIXELS; i++) {
        if (i) j += ",";
        j += String((int)lroundf(f[i] * 10.0f));   // tenths of a degree
    }
    j += "]}";
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", j);
}

void WebUI::handleGetSettings() {
    const Settings& c = settings.get();
    String j = "{";
    j += "\"wopen\":" + String(c.windowOpenMin) + ",";
    j += "\"wdead\":" + String(c.windowDeadlineMin) + ",";
    j += "\"stir\":" + String(c.stirringCounts) + ",";
    j += "\"awake\":" + String(c.awakeCounts) + ",";
    j += "\"scale\":" + String(c.countScale) + ",";
    j += "\"confirm\":" + String(c.awakeConfirmEpochs) + ",";
    j += "\"gvol\":" + String(c.gentleStartVol) + ",";
    j += "\"gramp\":" + String(c.gentleRampMin) + ",";
    j += "\"snz\":" + String(c.snoozeMin) + ",";
    j += "\"bl\":" + String(c.backlight) + ",";
    j += "\"need\":" + String(c.sleepNeedMin) + ",";
    j += "\"shift\":" + String(c.maxDebtShiftMin) + ",";
    j += "\"dae\":" + String(c.debtAwareEnabled ? "true" : "false") + ",";
    j += "\"bodym\":" + String(c.bodyMarginTenthC) + ",";
    j += "\"inbedpx\":" + String(c.inBedMinPixels) + ",";
    j += "\"aen\":" + String(c.alarmEnabled ? "true" : "false") + ",";
    j += "\"sen\":" + String(c.suppressEnabled ? "true" : "false");
    j += "}";
    _server.send(200, "application/json", j);
}

void WebUI::handleSetSettings() {
    if (_server.hasArg("defaults")) {
        settings.resetDefaults();
        display.setBrightness(settings.get().backlight);
        _server.send(200, "application/json", "{\"ok\":true}");
        return;
    }

    Settings& c = settings.get();
    if (_server.hasArg("wopen"))   c.windowOpenMin = _server.arg("wopen").toInt();
    if (_server.hasArg("wdead"))   c.windowDeadlineMin = _server.arg("wdead").toInt();
    if (_server.hasArg("stir"))    c.stirringCounts = _server.arg("stir").toInt();
    if (_server.hasArg("awake"))   c.awakeCounts = _server.arg("awake").toInt();
    if (_server.hasArg("scale"))   c.countScale = max(1L, _server.arg("scale").toInt());
    if (_server.hasArg("confirm")) c.awakeConfirmEpochs = _server.arg("confirm").toInt();
    if (_server.hasArg("gvol"))    c.gentleStartVol = _server.arg("gvol").toInt();
    if (_server.hasArg("gramp"))   c.gentleRampMin = max(1L, _server.arg("gramp").toInt());
    if (_server.hasArg("snz"))     c.snoozeMin = _server.arg("snz").toInt();
    if (_server.hasArg("bl"))      c.backlight = _server.arg("bl").toInt();
    if (_server.hasArg("need"))    c.sleepNeedMin = _server.arg("need").toInt();
    if (_server.hasArg("shift"))   c.maxDebtShiftMin = _server.arg("shift").toInt();
    if (_server.hasArg("dae"))     c.debtAwareEnabled = _server.arg("dae") == "1";
    if (_server.hasArg("bodym"))   c.bodyMarginTenthC = _server.arg("bodym").toInt();
    if (_server.hasArg("inbedpx")) c.inBedMinPixels = _server.arg("inbedpx").toInt();
    if (_server.hasArg("aen"))     c.alarmEnabled = _server.arg("aen") == "1";
    if (_server.hasArg("sen"))     c.suppressEnabled = _server.arg("sen") == "1";

    settings.save();
    display.setBrightness(c.backlight);
    thermal.setRoi(c.roiX0, c.roiY0, c.roiX1, c.roiY1);
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUI::handleAlarm() {
    String a = _server.arg("action");
    if (a == "dismiss")           alarmEngine.dismiss();
    else if (a == "snooze")       alarmEngine.snooze();
    else if (a == "test-gentle")  alarmEngine.testGentle();
    else if (a == "test-hard")    alarmEngine.testHard();
    else if (a == "stop")         alarmEngine.stopTest();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUI::handleScan() {
    int n = WiFi.scanNetworks();
    String j = "{\"nets\":[";
    for (int i = 0; i < n && i < 20; i++) {
        if (i) j += ",";
        String s = WiFi.SSID(i);
        s.replace("\\", "\\\\");
        s.replace("\"", "\\\"");
        j += "{\"ssid\":\"" + s + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    j += "]}";
    WiFi.scanDelete();
    _server.send(200, "application/json", j);
}

void WebUI::handleWifiSave() {
    if (!net.saveCredentials(_server.arg("ssid"), _server.arg("pass"))) {
        _server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    _server.send(200, "application/json", "{\"ok\":true}");
    _rebootPending = true;
    _rebootAtMs = millis() + 800;
}

void WebUI::handleForget() {
    net.forgetCredentials();
    _server.send(200, "application/json", "{\"ok\":true}");
    _rebootPending = true;
    _rebootAtMs = millis() + 800;
}

void WebUI::handleBuzzer() {
    uint16_t freq = _server.arg("freq").toInt();
    uint8_t vol = _server.arg("vol").toInt();
    uint16_t ms = _server.arg("ms").toInt();
    if (freq == 0 || vol == 0) buzzer.stop();
    else if (ms > 0) buzzer.beepBlocking(freq, vol, ms > 2000 ? 2000 : ms);
    else buzzer.playTone(freq, vol);
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUI::handleBacklight() {
    int v = constrain(_server.arg("v").toInt(), 0, 255);
    display.setBrightness((uint8_t)v);
    _server.send(200, "application/json", "{\"ok\":true}");
}
