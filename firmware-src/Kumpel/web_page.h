#pragma once
// Einstellungsseite, die der Kumpel selbst ausliefert (http://kumpel.local).
// Alles in einer Datei, keine externen Quellen - funktioniert auch im
// Einrichtungs-WLAN ohne Internet.
static const char WEB_PAGE[] = R"KUMPEL(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kumpel</title>
<style>
:root{--bg:#0e0f12;--card:#17191e;--line:#2a2d34;--text:#f2efe9;--muted:#9aa1ad;--acc:#5ee6d0;--warn:#ff8577;--ok:#62d39a}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:680px;margin:0 auto;padding:24px 16px 120px}
h1{font-size:1.7rem;margin:0}
h2{font-size:1.05rem;margin:0 0 12px}
.top{display:flex;gap:14px;align-items:center;margin-bottom:6px}
.face{width:56px;height:56px;border-radius:50%;background:#000;display:flex;gap:10px;align-items:center;justify-content:center;flex:none}
.face i{width:11px;height:24px;border-radius:6px;background:var(--acc)}
.sub{color:var(--muted);font-size:.9rem}
section{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:18px;margin-top:16px}
label{display:block;color:var(--muted);font-size:.85rem;margin:10px 0 4px}
input,select{width:100%;background:#0e0f12;color:var(--text);border:1px solid var(--line);border-radius:9px;padding:10px 11px;font:inherit;min-width:0}
input:focus,select:focus{outline:2px solid var(--acc);outline-offset:-1px}
input[type=range]{padding:0;accent-color:var(--acc)}
input[type=checkbox]{width:auto;accent-color:var(--acc);transform:scale(1.2);margin-right:8px}
button{appearance:none;border:0;border-radius:999px;padding:10px 18px;font:600 .95rem system-ui,sans-serif;cursor:pointer;background:#2a2d34;color:var(--text)}
button.pri{background:var(--acc);color:#06201c}
button.sm{padding:6px 11px;font-size:.85rem}
button:disabled{opacity:.5}
.row{display:flex;gap:10px;align-items:end}
.row>*{flex:1}
.msg{font-size:.9rem;margin-top:10px;min-height:1.2em}
.msg.ok{color:var(--ok)}.msg.err{color:var(--warn)}
.skins{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.skin{border:2px solid var(--line);border-radius:12px;padding:10px 6px;text-align:center;cursor:pointer;background:#0e0f12}
.skin.on{border-color:var(--acc)}
.skin svg{width:72px;height:72px;display:block;margin:0 auto 6px}
.item{border:1px solid var(--line);border-radius:12px;padding:12px;margin-top:10px}
.item .head{display:flex;gap:8px;align-items:center;justify-content:space-between}
.item .head b{font-size:.95rem}
.tools{display:flex;gap:6px}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:0 10px}
.hint{color:var(--muted);font-size:.85rem;margin:8px 0 0}
.bar{position:fixed;left:0;right:0;bottom:0;background:rgba(14,15,18,.94);border-top:1px solid var(--line);padding:12px 16px}
.bar div{max-width:680px;margin:0 auto;display:flex;gap:12px;align-items:center;justify-content:space-between}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;background:var(--muted)}
.dot.ok{background:var(--ok)}.dot.err{background:var(--warn)}
@media (max-width:480px){.grid2{grid-template-columns:1fr}}
</style></head><body><main>
<div class="top"><div class="face"><i></i><i></i></div>
<div><h1>Kumpel</h1><div class="sub" id="fw">Einstellungen</div></div></div>

<section><h2>Verbindung</h2>
<div><span class="dot" id="wdot"></span><span id="wtxt">WLAN …</span></div>
<div style="margin-top:4px"><span class="dot" id="hdot"></span><span id="htxt">Home Assistant …</span></div>
</section>

<section><h2>WLAN</h2>
<div class="row"><div><label>Netzwerk</label><input id="ssid" list="nets" autocomplete="off"><datalist id="nets"></datalist></div>
<div style="flex:0"><button class="sm" id="scan">Suchen</button></div></div>
<label>Passwort</label><input id="pass" type="password" placeholder="leer lassen = unverändert">
<div style="margin-top:12px"><button id="wsave">WLAN speichern</button></div>
<div class="msg" id="wmsg"></div>
</section>

<section><h2>Home Assistant</h2>
<label>Adresse</label><input id="haurl" placeholder="http://homeassistant.local:8123">
<label>Langlebiges Zugriffstoken</label><input id="hatok" type="password" placeholder="Profil → Sicherheit → Token erstellen">
<p class="hint">Tipp: einen eigenen Benutzer für den Kumpel anlegen – dann lässt sich der Zugang jederzeit einzeln widerrufen. Das Token wird nur auf dem Gerät gespeichert und hier nie wieder angezeigt.</p>
<div style="margin-top:12px"><button id="hsave">Testen &amp; speichern</button></div>
<div class="msg" id="hmsg"></div>
</section>

<section><h2>Figur</h2>
<div class="skins" id="skins"></div>
<p class="hint">Am Gerät: lange auf den Kumpel drücken wechselt die Figur.</p>
</section>

<section><h2>Seiten <span class="sub">(wischen am Gerät)</span></h2>
<div id="pages"></div>
<div style="margin-top:12px;display:flex;gap:10px;align-items:center"><button class="sm" id="padd">+ Seite</button><span class="sub" id="elist"></span></div>
</section>

<section><h2>Reaktionen</h2>
<label>Friert bei Kälte – Temperatur von</label><input id="cold" list="ents_temp" placeholder="z. B. weather.forecast_home oder ein Temperatursensor (leer = aus)">
<label>… unter (°C)</label><input id="coldb" type="number" step="0.5">
<label style="margin-top:16px">Wetter (Sonnenbrille bei Sonne, Regenwolke bei Regen)</label><input id="weather" list="ents_weather" placeholder="z. B. weather.forecast_home (leer = aus)">
<label>Schwitzt ab Außentemperatur (°C)</label><input id="hot" type="number" step="0.5">
<label style="margin-top:16px"><input type="checkbox" id="sleepon">Nachts schlafen und dimmen (vorher wird er müde und gähnt)</label>
<div class="grid2"><div><label>von</label><input id="sfrom" type="time"></div><div><label>bis</label><input id="sto" type="time"></div></div>
<h2 style="margin-top:20px">Hinweise <span class="sub">(Sprechblase)</span></h2>
<div id="hints"></div>
<div style="margin-top:12px"><button class="sm" id="hadd">+ Hinweis</button></div>
<p class="hint">Tipp: <b>{}</b> im Text wird durch den aktuellen Wert ersetzt. Beispiel: Entity „sensor.abfall_morgen“, Bedingung „ist nicht“, Wert „nichts“, Text „Morgen: {}“ → „Morgen: Papier, Bio“.<br>Geburtstage: Entity „calendar.geburtstage“, Bedingung „ist gleich“, Wert „on“, Text „Heute: {}“ → Titel des Termins.</p>
</section>

<section><h2>Anzeige</h2>
<div class="grid2">
<div><label>Helligkeit tagsüber <span id="bdv"></span></label><input id="bd" type="range" min="20" max="255"></div>
<div><label>Helligkeit nachts <span id="bnv"></span></label><input id="bn" type="range" min="0" max="160"></div></div>
<label>Zurück zum Kumpel nach (Sekunden)</label><input id="home" type="number" min="10" max="250">
<label>Bild drehen</label><select id="rot">
<option value="auto">automatisch (Lagesensor)</option>
<option value="0">nicht drehen</option>
<option value="90">90° im Uhrzeigersinn</option>
<option value="180">180° (auf dem Kopf)</option>
<option value="270">90° gegen den Uhrzeigersinn</option>
</select>
<p class="hint">„Automatisch“ dreht das Bild so, wie das Gerät gerade steht – z. B. im Figuren-Ständer mit USB-C nach unten. Liegt es flach, bleibt die letzte Lage. Touch und Wischen drehen mit. <span id="rotnow"></span></p>
<label>Zeitzone</label><select id="tz">
<option value="CET-1CEST,M3.5.0,M10.5.0/3">Deutschland, Österreich, Schweiz …</option>
<option value="GMT0BST,M3.5.0/1,M10.5.0">Großbritannien, Irland</option>
<option value="WET0WEST,M3.5.0/1,M10.5.0">Portugal</option>
<option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Osteuropa (Finnland, Griechenland …)</option>
<option value="EST5EDT,M3.2.0,M11.1.0">USA Ostküste</option>
<option value="PST8PDT,M3.2.0,M11.1.0">USA Westküste</option>
</select>
</section>
</main>
<div class="bar"><div><span class="msg" id="smsg"></span><button class="pri" id="save">Speichern</button></div></div>
<template id="tpage"><div class="item"><div class="head"><b></b><div class="tools">
<button class="sm up" title="nach oben">↑</button><button class="sm dn" title="nach unten">↓</button><button class="sm del" title="entfernen">✕</button></div></div>
<div class="grid2"><div><label>Art</label><select class="type">
<option value="climate">Heizung / Klima</option><option value="light">Licht</option><option value="switch">Schalter</option>
<option value="sensor">Anzeige (Sensor)</option><option value="button">Szene / Skript / Taste</option><option value="vacuum">Saugroboter</option></select></div>
<div><label>Entity</label><input class="ent" autocomplete="off"></div></div>
<label>Titel (optional)</label><input class="title" maxlength="30" placeholder="Name aus Home Assistant">
</div></template>
<template id="thint"><div class="item"><div class="head"><b>Hinweis</b><div class="tools"><button class="sm del">✕</button></div></div>
<label>Entity</label><input class="ent" list="ents_all" autocomplete="off">
<div class="grid2"><div><label>Bedingung</label><select class="cond">
<option value="eq">ist gleich</option><option value="ne">ist nicht</option><option value="contains">enthält</option>
<option value="lt">kleiner als</option><option value="gt">größer als</option></select></div>
<div><label>Wert</label><input class="val" maxlength="30"></div></div>
<label>Text in der Sprechblase</label><input class="txt" maxlength="38">
</div></template>
<script>
const $=id=>document.getElementById(id);
const DOM={climate:['climate'],light:['light'],switch:['switch','input_boolean','fan','light','cover','automation','siren','humidifier'],
 sensor:null,button:['scene','script','button','input_button','automation'],vacuum:['vacuum']};
const SKINS=[['eyes','Leuchtaugen','<rect x="18" y="20" width="13" height="32" rx="6.5" fill="#5ee6d0"/><rect x="41" y="20" width="13" height="32" rx="6.5" fill="#5ee6d0"/>'],
 ['drop','Tröpfchen','<path d="M36 10C40 20 54 30 54 44A18 18 0 0 1 18 44C18 30 32 20 36 10Z" fill="#3fa9d6"/><circle cx="30" cy="46" r="2.6" fill="#0b2a3a"/><circle cx="42" cy="46" r="2.6" fill="#0b2a3a"/><path d="M32 52Q36 55 40 52" stroke="#0b2a3a" stroke-width="2" fill="none" stroke-linecap="round"/>'],
 ['pebble','Kiesel','<ellipse cx="36" cy="40" rx="26" ry="19" fill="#e8d3a8"/><circle cx="29" cy="38" r="2.6" fill="#4a3b22"/><circle cx="43" cy="38" r="2.6" fill="#4a3b22"/><path d="M32 44Q36 47 40 44" stroke="#4a3b22" stroke-width="2" fill="none" stroke-linecap="round"/>']];
let cfg={},ents=[];
const api=(p,b)=>fetch(p,b===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)}).then(r=>r.json());
function msg(id,t,ok){const e=$(id);e.textContent=t;e.className='msg '+(ok===undefined?'':ok?'ok':'err')}

function lists(){
  let html='';
  const mk=(id,f)=>{let o=ents.filter(f).map(e=>`<option value="${e.i}">${esc(e.n)}${e.u?' ('+esc(e.s)+' '+esc(e.u)+')':''}</option>`).join('');html+=`<datalist id="${id}">${o}</datalist>`};
  for(const t in DOM)mk('ents_'+t,e=>!DOM[t]||DOM[t].includes(e.i.split('.')[0]));
  mk('ents_temp',e=>e.i.startsWith('climate.')||e.i.startsWith('weather.')||/°/.test(e.u||''));
  mk('ents_weather',e=>e.i.startsWith('weather.'));
  mk('ents_all',()=>true);
  let d=$('dl');if(!d){d=document.createElement('div');d.id='dl';document.body.appendChild(d)}d.innerHTML=html;
}
function esc(s){return String(s||'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function name(id){const e=ents.find(x=>x.i===id);return e&&e.n?e.n:''}

function renderSkins(){$('skins').innerHTML=SKINS.map(([k,n,svg])=>`<div class="skin ${cfg.skin===k?'on':''}" data-k="${k}"><svg viewBox="0 0 72 72"><circle cx="36" cy="36" r="36" fill="#000"/>${svg}</svg>${n}</div>`).join('');
  document.querySelectorAll('.skin').forEach(el=>el.onclick=()=>{cfg.skin=el.dataset.k;renderSkins()})}

function renderPages(){
  const box=$('pages');box.innerHTML='';
  cfg.pages.forEach((p,i)=>{
    const n=$('tpage').content.cloneNode(true),el=n.firstElementChild;
    el.querySelector('b').textContent='Seite '+(i+1)+(name(p.entity)?' – '+name(p.entity):'');
    const ty=el.querySelector('.type'),en=el.querySelector('.ent'),ti=el.querySelector('.title');
    ty.value=p.type;en.value=p.entity;ti.value=p.title||'';en.setAttribute('list','ents_'+p.type);
    ty.onchange=()=>{p.type=ty.value;en.setAttribute('list','ents_'+p.type)};
    en.oninput=()=>{p.entity=en.value.trim();el.querySelector('b').textContent='Seite '+(i+1)+(name(p.entity)?' – '+name(p.entity):'')};
    ti.oninput=()=>p.title=ti.value;
    el.querySelector('.up').onclick=()=>{if(i){cfg.pages.splice(i-1,0,cfg.pages.splice(i,1)[0]);renderPages()}};
    el.querySelector('.dn').onclick=()=>{if(i<cfg.pages.length-1){cfg.pages.splice(i+1,0,cfg.pages.splice(i,1)[0]);renderPages()}};
    el.querySelector('.del').onclick=()=>{cfg.pages.splice(i,1);renderPages()};
    box.appendChild(n);
  });
  $('padd').disabled=cfg.pages.length>=8;
}
function renderHints(){
  const box=$('hints');box.innerHTML='';
  cfg.hints.forEach((h,i)=>{
    const n=$('thint').content.cloneNode(true),el=n.firstElementChild;
    const f=(c,k)=>{const x=el.querySelector(c);x.value=h[k]||'';x.oninput=x.onchange=()=>h[k]=x.value.trim()};
    f('.ent','entity');f('.cond','cond');f('.val','value');f('.txt','text');
    if(!h.cond)el.querySelector('.cond').value='eq';
    el.querySelector('.del').onclick=()=>{cfg.hints.splice(i,1);renderHints()};
    box.appendChild(n);
  });
  $('hadd').disabled=cfg.hints.length>=4;
}
function fill(){
  renderSkins();renderPages();renderHints();
  $('cold').value=cfg.cold.entity||'';$('coldb').value=cfg.cold.below;
  const wt=cfg.weather||{};$('weather').value=wt.entity||'';$('hot').value=wt.hot??28;
  $('sleepon').checked=cfg.sleep.on;$('sfrom').value=cfg.sleep.from;$('sto').value=cfg.sleep.to;
  $('bd').value=cfg.bright.day;$('bn').value=cfg.bright.night;$('home').value=cfg.homeAfter;$('rot').value=String(cfg.rotate??'auto');
  $('bdv').textContent=cfg.bright.day;$('bnv').textContent=cfg.bright.night;
}
function collect(){
  cfg.cold={entity:$('cold').value.trim(),below:parseFloat($('coldb').value)||18};
  cfg.weather={entity:$('weather').value.trim(),hot:parseFloat($('hot').value)||28};
  cfg.sleep={on:$('sleepon').checked,from:$('sfrom').value||'23:00',to:$('sto').value||'07:00'};
  cfg.bright={day:+$('bd').value,night:+$('bn').value};
  cfg.homeAfter=+$('home').value||45;
  cfg.rotate=$('rot').value==='auto'?'auto':+$('rot').value;
  cfg.pages=cfg.pages.filter(p=>p.entity);
  cfg.hints=cfg.hints.filter(h=>h.entity&&h.text);
}
function status(){
  api('/api/status').then(s=>{
    $('fw').textContent='Firmware '+s.fw+(s.ip?' · '+s.ip:'');
    const w=s.wifi;$('wdot').className='dot '+(w.online?'ok':'err');
    $('wtxt').textContent=w.online?`WLAN „${w.ssid}“ (${w.rssi} dBm)`:(w.ap?'Einrichtungs-WLAN aktiv':'WLAN nicht verbunden');
    const st={ready:['ok','Home Assistant verbunden'],connecting:['','Verbinde mit Home Assistant …'],auth:['err','Token wird abgelehnt'],off:['err','Home Assistant noch nicht eingerichtet']}[s.ha.state];
    $('hdot').className='dot '+st[0];$('htxt').textContent=st[1]+(s.ha.error&&s.ha.state!=='ready'?' – '+s.ha.error:'')+(s.ha.drops?` · ${s.ha.drops} Abbrüche, zuletzt: ${s.ha.error||'?'}`:'');
    if(!$('ssid').value)$('ssid').value=w.ssid||'';
    if(!$('haurl').value)$('haurl').value=s.ha.url||'';
    $('hatok').placeholder=s.ha.token?'gespeichert – leer lassen zum Behalten':'Profil → Sicherheit → Token erstellen';
    $('tz').value=s.tz;
    if(s.rot!==undefined)$('rotnow').textContent=`Gerade angezeigt: ${s.rot}°.`;
  }).catch(()=>{});
}
function loadEnts(){
  $('elist').textContent='Entities werden geladen …';
  api('/api/entities').then(r=>{
    if(r.error){$('elist').textContent=r.error;return}
    ents=r.sort((a,b)=>a.i.localeCompare(b.i));lists();renderPages();
    $('elist').textContent=ents.length+' Entities aus Home Assistant';
  }).catch(()=>$('elist').textContent='Entity-Liste nicht verfügbar – Namen von Hand eintragen');
}
$('padd').onclick=()=>{cfg.pages.push({type:'light',entity:'',title:''});renderPages()};
$('hadd').onclick=()=>{cfg.hints.push({entity:'',cond:'eq',value:'',text:''});renderHints()};
$('bd').oninput=()=>$('bdv').textContent=$('bd').value;$('bn').oninput=()=>$('bnv').textContent=$('bn').value;
$('scan').onclick=()=>{$('scan').disabled=true;api('/api/scan').then(l=>{$('nets').innerHTML=l.map(n=>`<option value="${esc(n.s)}">${n.r} dBm${n.l?' 🔒':''}</option>`).join('');$('scan').disabled=false}).catch(()=>$('scan').disabled=false)};
$('wsave').onclick=()=>{api('/api/wifi',{ssid:$('ssid').value.trim(),pass:$('pass').value}).then(r=>msg('wmsg',r.msg,r.ok))};
$('hsave').onclick=()=>{msg('hmsg','Teste …');$('hsave').disabled=true;
  api('/api/ha',{url:$('haurl').value.trim(),token:$('hatok').value.trim()}).then(r=>{msg('hmsg',r.msg,r.ok);$('hsave').disabled=false;if(r.ok){$('hatok').value='';status();loadEnts()}})
  .catch(()=>{msg('hmsg','Keine Antwort vom Gerät',false);$('hsave').disabled=false})};
$('save').onclick=()=>{collect();
  Promise.all([api('/api/config',cfg),api('/api/tz',{tz:$('tz').value})]).then(([r])=>{msg('smsg',r.msg,r.ok);if(r.ok){renderPages();renderHints()}})
  .catch(()=>msg('smsg','Keine Antwort vom Gerät',false))};
api('/api/config').then(c=>{cfg=c;fill()});
status();setInterval(status,5000);loadEnts();
</script></body></html>
)KUMPEL";
