#pragma once

// Single-page web UI, served from flash. Polls /api/state once a second
// and posts commands to /api/cmd. This file is UTF-8; keep it that way.

const char INDEX_HTML[] PROGMEM = R"PAGE(<!doctype html>
<html lang="vi"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bơm nước</title>
<style>
:root{--bg:#f4f4f2;--card:#fff;--tx:#1a1a19;--tx2:#6b6b66;--bd:#dedcd4;
--ok:#1d9e75;--off:#9a9a94;--err:#c43b3b;--acc:#378add}
@media(prefers-color-scheme:dark){:root{--bg:#161614;--card:#222220;--tx:#f0efe9;
--tx2:#9a9a92;--bd:#38382f;--ok:#5dcaa5;--off:#6b6b64;--err:#f09595}}
*{box-sizing:border-box}
body{margin:0;padding:16px;background:var(--bg);color:var(--tx);
font:16px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:560px;margin:0 auto}
h1{font-size:20px;font-weight:500;margin:0 0 4px}
.sub{color:var(--tx2);font-size:13px;margin-bottom:16px}
.card{background:var(--card);border:1px solid var(--bd);border-radius:12px;
padding:14px 16px;margin-bottom:12px}
h2{font-size:15px;font-weight:500;margin:0 0 10px}
.row{display:flex;justify-content:space-between;align-items:center;
padding:5px 0;font-size:14px;gap:12px}
.row span{color:var(--tx2)}
.row b{font-weight:500;text-align:right}
.pill{display:inline-block;padding:2px 10px;border-radius:999px;
font-size:13px;font-weight:500;color:#fff}
.btns{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap}
button{flex:1;min-width:84px;padding:10px 8px;border-radius:8px;
border:1px solid var(--bd);background:var(--bg);color:var(--tx);
font-size:14px;font-family:inherit;cursor:pointer}
button.on{background:var(--acc);border-color:var(--acc);color:#fff}
button:active{opacity:.7}
.alarm{background:var(--err);color:#fff;border-radius:12px;padding:12px 16px;
margin-bottom:12px;font-size:14px}
.alarm button{background:rgba(255,255,255,.2);border-color:rgba(255,255,255,.45);
color:#fff;margin-top:10px}
.hide{display:none}
.bar{height:8px;background:var(--bd);border-radius:999px;overflow:hidden;margin:8px 0 2px}
.bar i{display:block;height:100%;background:var(--acc);width:0}
input[type=range]{width:100%}
input[type=file],input[type=password],input[type=text],input[type=number],select{width:100%;margin:6px 0;
padding:9px 10px;border:1px solid var(--bd);border-radius:8px;background:var(--bg);
color:var(--tx);font-size:14px;font-family:inherit}
.note{color:var(--tx2);font-size:13px;margin-top:8px}
.io{display:flex;justify-content:space-between;align-items:center;padding:6px 0;
font-size:13.5px;border-bottom:1px solid var(--bd)}
.io:last-child{border-bottom:none}
.io .nm{color:var(--tx2)}
.io .gp{color:var(--tx2);font-size:12px;margin-left:6px;opacity:.75}
.led{display:inline-block;width:10px;height:10px;border-radius:50%;
background:var(--off);margin-right:7px;vertical-align:-1px}
.led.hi{background:var(--ok)}
</style></head><body><div class="wrap">

<h1>Hệ thống bơm nước</h1>
<div class="sub"><span id="conn">đang kết nối…</span> · <span id="up">—</span></div>

<div id="alarm" class="alarm hide">
  <div id="alarmTx"></div>
  <button onclick="cmd('clear=1')">Xoá lỗi và mở khoá</button>
</div>

<div class="card">
  <h2>Bồn mái</h2>
  <div class="bar"><i id="lvlBar"></i></div>
  <div class="row"><span>Mực nước</span><b id="lvl">—</b></div>
  <div class="row"><span>Phao mức thấp</span><b id="fLow">—</b></div>
  <div class="row"><span>Phao mức đầy</span><b id="fFull">—</b></div>
  <div id="temps"></div>
</div>

<div class="card">
  <h2>Bơm lên bồn</h2>
  <div class="row"><span>Trạng thái</span><b><span id="tankPill" class="pill"></span></b></div>
  <div class="row"><span>Chế độ</span><b id="tankMode">—</b></div>
  <div class="row"><span>Đã chạy</span><b id="tankRun">—</b></div>
  <div class="row"><span>Lưu lượng</span><b id="fTank">—</b></div>
  <div class="btns">
    <button id="bAuto" onclick="cmd('tank=auto')">Tự động</button>
    <button id="bOn"   onclick="cmd('tank=on')">Bật tay</button>
    <button id="bOff"  onclick="cmd('tank=off')">Tắt</button>
  </div>
</div>

<div class="card">
  <h2>Bơm áp</h2>
  <div class="row"><span>Trạng thái</span><b><span id="bstPill" class="pill"></span></b></div>
  <div class="row"><span>Lưu lượng</span><b id="fBoost">—</b></div>
  <div class="btns">
    <button id="bBstOn"  onclick="cmd('booster=on')">Bật</button>
    <button id="bBstOff" onclick="cmd('booster=off')">Tắt</button>
  </div>
</div>

<div class="card">
  <h2>Đường nước</h2>
  <div class="row"><span>Van bi</span><b id="valve">—</b></div>
  <div class="btns">
    <button id="bVclose" onclick="cmd('valve=close')">Qua bơm áp</button>
    <button id="bVopen"  onclick="cmd('valve=open')">Chảy thường</button>
  </div>
</div>

<div class="card">
  <h2>Cài đặt bảo vệ bơm lên bồn</h2>
  <div class="row"><span>Kiểm tra chạy khô (giây)</span><b id="cfgDryTx">45 giây</b></div>
  <input type="number" id="cfgDry" min="5" max="600" step="1" value="45" placeholder="45"
         oninput="this.dataset.dirty='1';if(this.value)$('cfgDryTx').textContent=this.value+' giây'">
  <div class="note">Sau khi bơm chạy đủ số giây này mới kiểm tra lưu lượng để phát hiện chạy khô.</div>

  <div class="row" style="margin-top:10px"><span>Nghỉ tối thiểu sau dừng (phút)</span><b id="cfgRestTx">2 phút</b></div>
  <input type="number" id="cfgRest" min="0.1" max="180" step="0.5" value="2" placeholder="2"
         oninput="this.dataset.dirty='1';if(this.value)$('cfgRestTx').textContent=this.value+' phút'">
  <div class="note">Sau khi vừa dừng, phải nghỉ tối thiểu số phút này trước khi được chạy lại.</div>

  <div class="row" style="margin-top:10px"><span>Chạy liên tục tối đa (phút)</span><b id="cfgRunTx">45 phút</b></div>
  <input type="number" id="cfgRun" min="1" max="360" step="1" value="45" placeholder="45"
         oninput="this.dataset.dirty='1';if(this.value)$('cfgRunTx').textContent=this.value+' phút'">
  <div class="note">Bơm chạy liên tục tối đa số phút này, sau đó tự dừng để bảo vệ.</div>

  <div class="btns" style="margin-top:12px">
    <button onclick="saveTimings()">Lưu thời gian bảo vệ</button>
  </div>
  <div class="note" id="cfgMsg"></div>
</div>

<div class="card">
  <h2>Trạng thái chân ESP32</h2>
  <div id="ioList"></div>
  <div class="note">Đèn xanh là chân đang tích cực. 4 chân relay là đầu ra, 2 chân phao là đầu vào.</div>
</div>

<div class="card" id="simCard">
  <h2>Mô phỏng cảm biến</h2>
  <div class="btns">
    <button id="bSimLow"  onclick="tog('aboveLow')">Bồn đã cạn</button>
    <button id="bSimFull" onclick="tog('isFull')">Bồn đã đầy</button>
  </div>
  <div class="row" style="margin-top:10px"><span>Lưu lượng giả lập</span><b id="simFlowTx">0 L/phút</b></div>
  <input type="range" id="simFlow" min="0" max="40" value="0"
         oninput="document.getElementById('simFlowTx').textContent=this.value+' L/phút'"
         onchange="cmd('simFlow='+this.value)">
  <div class="btns"><button onclick="cmd('sim=0')">Tắt mô phỏng</button></div>
</div>

<div class="card hide" id="realCard">
  <h2>Cảm biến thật</h2>
  <div class="btns"><button onclick="cmd('sim=1')">Bật lại mô phỏng</button></div>
</div>

<div class="card">
  <h2>Cập nhật firmware</h2>
  <div class="row"><span>Phiên bản đang chạy</span><b id="fw">—</b></div>
  <input type="file" id="binf" accept=".bin">
  <input type="password" id="otapw" placeholder="Mật khẩu cập nhật" autocomplete="off">
  <div class="bar"><i id="otaBar"></i></div>
  <div class="note" id="otaTx">Chọn file .bin rồi bấm nạp. Bơm sẽ tự tắt trong lúc cập nhật.</div>
  <div class="btns"><button onclick="doOta()">Nạp firmware</button></div>
</div>

<div class="card">
  <h2>Cài đặt WiFi</h2>
  <div class="row"><span>Đang nối</span><b id="wSsid">—</b></div>
  <div class="row"><span>Địa chỉ IP</span><b id="wIp">—</b></div>
  <select id="wList"><option value="">— bấm Quét mạng —</option></select>
  <input type="text" id="wSsidIn" placeholder="Hoặc gõ tay tên WiFi" autocapitalize="off" autocorrect="off">
  <input type="password" id="wPass" placeholder="Mật khẩu WiFi" autocomplete="off">
  <input type="password" id="wAuth" placeholder="Mật khẩu cập nhật (như phần OTA)" autocomplete="off">
  <div class="btns">
    <button onclick="scanWifi()">Quét mạng</button>
    <button onclick="saveWifi()">Lưu và khởi động lại</button>
  </div>
  <div class="btns"><button onclick="forgetWifi()">Quên WiFi đã lưu</button></div>
  <div class="note" id="wTx">Đổi WiFi không cần nạp lại firmware. Nếu mạng mới sai mật khẩu,
  bo sẽ tự phát sóng riêng để bạn vào sửa lại.</div>
</div>

<script>
var S={};
function $(i){return document.getElementById(i)}
function css(n){return getComputedStyle(document.documentElement).getPropertyValue(n)}
function hms(s){var h=(s/3600)|0,m=((s%3600)/60)|0,x=s%60;
  return (h?h+'g ':'')+(h||m?m+'p ':'')+x+'s'}
function sel(el,c){el.className=c?'on':''}

function render(s){
  S=s;
  $('conn').textContent='đã kết nối · '+s.rssi+' dBm';
  $('up').textContent='chạy '+hms(s.up);

  if(s.fault){$('alarm').classList.remove('hide');$('alarmTx').textContent=s.fault}
  else $('alarm').classList.add('hide');

  if(s.lvl>=0){$('lvl').textContent=s.lvl.toFixed(0)+'% · '+s.litres.toFixed(0)+' lít';
    $('lvlBar').style.width=s.lvl+'%'}
  else{$('lvl').textContent='chưa lắp cảm biến';$('lvlBar').style.width='0'}

  $('fLow').textContent  = s.aboveLow ? 'còn trên mức thấp' : 'ĐÃ CẠN';
  $('fFull').textContent = s.isFull   ? 'ĐÃ ĐẦY' : 'chưa đầy';
  $('temps').innerHTML=(s.temps||[]).map(function(t){
    var v = t.v>-100 ? t.v.toFixed(1)+' °C' : 'mất kết nối';
    return '<div class="row"><span>'+t.n+'</span><b>'+v+'</b></div>';
  }).join('');

  $('tankPill').textContent=s.tankOn?'ĐANG CHẠY':'tắt';
  $('tankPill').style.background=s.tankOn?css('--ok'):css('--off');
  $('tankMode').textContent=s.tankMode=='auto'?'tự động':(s.tankMode=='on'?'bật tay':'tắt tay');
  $('tankRun').textContent=s.tankOn?hms(s.tankRun):'—';
  $('fTank').textContent=s.fTank.toFixed(1)+' L/phút';
  sel($('bAuto'),s.tankMode=='auto');sel($('bOn'),s.tankMode=='on');sel($('bOff'),s.tankMode=='off');

  $('bstPill').textContent=s.boostOn?'ĐANG BẬT':'tắt';
  $('bstPill').style.background=s.boostOn?css('--ok'):css('--off');
  $('fBoost').textContent=s.fBoost.toFixed(1)+' L/phút';
  sel($('bBstOn'),s.boostOn);sel($('bBstOff'),!s.boostOn);

  $('valve').textContent = s.valve=='open'?'MỞ · nước chảy thường'
                         :(s.valve=='closed'?'ĐÓNG · nước qua bơm áp':'đang quay…');
  sel($('bVopen'),s.valve=='open');sel($('bVclose'),s.valve=='closed');

  if(s.dryCheck!==undefined){
    $('cfgDryTx').textContent=s.dryCheck+' giây';
    if(document.activeElement!==$('cfgDry')&&!$('cfgDry').dataset.dirty)$('cfgDry').value=s.dryCheck;
  }
  if(s.minRest!==undefined){
    $('cfgRestTx').textContent=s.minRest+' phút';
    if(document.activeElement!==$('cfgRest')&&!$('cfgRest').dataset.dirty)$('cfgRest').value=s.minRest;
  }
  if(s.maxRun!==undefined){
    $('cfgRunTx').textContent=s.maxRun+' phút';
    if(document.activeElement!==$('cfgRun')&&!$('cfgRun').dataset.dirty)$('cfgRun').value=s.maxRun;
  }

  $('fw').textContent='v'+s.fw;
  $('wSsid').textContent=s.ssid||'—';
  $('wIp').textContent=s.ip||'—';

  $('ioList').innerHTML=(s.io||[]).map(function(p){
    return '<div class="io"><span class="nm">'+p.n+'<span class="gp">GPIO '+p.g+'</span></span>'
         + '<b><span class="led'+(p.v?' hi':'')+'"></span>'+(p.v?'bật':'tắt')+'</b></div>';
  }).join('');

  $('simCard').classList.toggle('hide',!s.sim);
  $('realCard').classList.toggle('hide',!!s.sim);
  sel($('bSimLow'),!s.aboveLow);sel($('bSimFull'),s.isFull);
}

async function poll(){
  try{var r=await fetch('/api/state');render(await r.json())}
  catch(e){$('conn').textContent='mất kết nối'}
}
async function cmd(q){
  try{var r=await fetch('/api/cmd?'+q);render(await r.json())}
  catch(e){poll()}
}
function tog(k){cmd((k=='aboveLow'?'simAboveLow=':'simIsFull=')+((k=='aboveLow'?S.aboveLow:S.isFull)?0:1))}

function saveTimings(){
  var rawD = $('cfgDry').value ? $('cfgDry').value.trim() : '';
  var rawR = $('cfgRest').value ? $('cfgRest').value.trim() : '';
  var rawM = $('cfgRun').value ? $('cfgRun').value.trim() : '';
  var d = parseFloat(rawD !== '' ? rawD : $('cfgDry').placeholder);
  var r = parseFloat(rawR !== '' ? rawR : $('cfgRest').placeholder);
  var m = parseFloat(rawM !== '' ? rawM : $('cfgRun').placeholder);

  if(isNaN(d) || d < 5 || d > 600){
    $('cfgMsg').style.color = 'var(--err)';
    $('cfgMsg').textContent = 'Thời gian kiểm tra chạy khô phải từ 5 đến 600 giây';
    return;
  }
  if(isNaN(r) || r < 0.1 || r > 180){
    $('cfgMsg').style.color = 'var(--err)';
    $('cfgMsg').textContent = 'Thời gian nghỉ phải từ 0.1 đến 180 phút';
    return;
  }
  if(isNaN(m) || m < 1 || m > 360){
    $('cfgMsg').style.color = 'var(--err)';
    $('cfgMsg').textContent = 'Thời gian chạy tối đa phải từ 1 đến 360 phút';
    return;
  }
  
  $('cfgDryTx').textContent = d + ' giây';
  $('cfgRestTx').textContent = r + ' phút';
  $('cfgRunTx').textContent = m + ' phút';
  $('cfgDry').value = d;
  $('cfgRest').value = r;
  $('cfgRun').value = m;
  $('cfgDry').dataset.dirty = '';
  $('cfgRest').dataset.dirty = '';
  $('cfgRun').dataset.dirty = '';
  cmd('cfgDry=' + encodeURIComponent(d) + '&cfgRest=' + encodeURIComponent(r) + '&cfgRun=' + encodeURIComponent(m));
  $('cfgMsg').style.color = 'var(--ok)';
  $('cfgMsg').textContent = 'Đã lưu cài đặt bảo vệ thành công!';
  setTimeout(function(){ $('cfgMsg').textContent = ''; }, 4000);
}

function doOta(){
  var f=$('binf').files[0], pw=$('otapw').value;
  if(!f){$('otaTx').textContent='Chưa chọn file .bin';return}
  if(!pw){$('otaTx').textContent='Chưa nhập mật khẩu cập nhật';return}
  // ESP32 phục vụ một kết nối tại một thời điểm, phải dừng hỏi trạng thái
  clearInterval(timer);
  var fd=new FormData(); fd.append('fw',f,f.name);
  var x=new XMLHttpRequest();
  x.open('POST','/api/ota');
  x.setRequestHeader('X-OTA-Password',pw);
  x.upload.onprogress=function(e){
    if(!e.lengthComputable)return;
    var p=e.loaded/e.total*100;
    $('otaBar').style.width=p+'%';
    $('otaTx').textContent='Đang tải lên '+p.toFixed(0)+'%, đừng tắt nguồn';
  };
  x.onload=function(){
    if(x.status==200){
      $('otaBar').style.width='100%';
      $('otaTx').textContent='Xong. Bo đang khởi động lại, chờ khoảng 10 giây rồi tải lại trang.';
      setTimeout(function(){location.reload()},12000);
    }else{
      $('otaBar').style.width='0';
      var m='Lỗi';try{m=JSON.parse(x.responseText).msg||m}catch(e){}
      $('otaTx').textContent=m;
      timer=setInterval(poll,1000);
    }
  };
  x.onerror=function(){
    $('otaBar').style.width='0';
    $('otaTx').textContent='Mất kết nối khi đang tải lên. Thử lại.';
    timer=setInterval(poll,1000);
  };
  x.send(fd);
}

async function scanWifi(){
  $('wTx').textContent='Đang quét, chờ vài giây…';
  try{
    var r=await fetch('/api/scan'), l=await r.json(), sel=$('wList');
    sel.innerHTML='';
    var o0=document.createElement('option');
    o0.value=''; o0.textContent=l.length?'— chọn mạng —':'— không thấy mạng nào —';
    sel.appendChild(o0);
    l.forEach(function(n){
      var o=document.createElement('option');
      o.value=n.s; o.textContent=n.s+'   ('+n.r+' dBm)';
      sel.appendChild(o);
    });
    $('wTx').textContent='Thấy '+l.length+' mạng';
  }catch(e){ $('wTx').textContent='Quét không được, thử lại' }
}

async function saveWifi(){
  var ssid=$('wList').value || $('wSsidIn').value.trim();
  if(!ssid){ $('wTx').textContent='Chưa chọn hoặc chưa gõ tên WiFi'; return }
  if(!$('wAuth').value){ $('wTx').textContent='Chưa nhập mật khẩu cập nhật'; return }
  var q='ssid='+encodeURIComponent(ssid)
      +'&pass='+encodeURIComponent($('wPass').value)
      +'&pw='+encodeURIComponent($('wAuth').value);
  await sendWifi(q,'Đã lưu "'+ssid+'". Bo đang khởi động lại. Nối điện thoại vào mạng đó rồi mở lại trang.');
}

async function forgetWifi(){
  if(!$('wAuth').value){ $('wTx').textContent='Chưa nhập mật khẩu cập nhật'; return }
  await sendWifi('forget=1&pw='+encodeURIComponent($('wAuth').value),
    'Đã quên WiFi. Bo khởi động lại và sẽ dùng mạng ghi trong config.h, hoặc tự phát sóng riêng.');
}

async function sendWifi(q,okMsg){
  clearInterval(timer);
  $('wTx').textContent='Đang gửi…';
  try{
    var r=await fetch('/api/wifi?'+q), j=await r.json();
    if(j.ok) $('wTx').textContent=okMsg;
    else { $('wTx').textContent=j.msg||'Lỗi'; timer=setInterval(poll,1000) }
  }catch(e){
    // bo khởi động lại thì kết nối đứt giữa chừng, đó là chuyện bình thường
    $('wTx').textContent=okMsg;
  }
}

poll();var timer=setInterval(poll,1000);
</script>
</div></body></html>
)PAGE";
