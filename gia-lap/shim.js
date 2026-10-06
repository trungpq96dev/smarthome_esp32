// Thay thu vien mqtt.js bang mot ban gia. Trang dieu-khien.html khong bi sua
// mot chu nao: cac lenh publish duoc chuyen thanh goi /api/cmd, con trang thai
// lay tu /api/state roi phat lai duoi dang ban tin MQTT.
//
// Nho vay trang tu xa va trang noi bo dung CHUNG mot con ESP32 gia lap.

(function () {
  var T_STATE = 'bomnuoc/state', T_CMD = 'bomnuoc/cmd', T_STATUS = 'bomnuoc/status';

  try {
    localStorage.setItem('mq_host', 'esp32-gia-lap');
    localStorage.setItem('mq_port', '8884');
    localStorage.setItem('mq_user', 'phone');
    localStorage.setItem('mq_pass', 'x');
  } catch (e) {}

  window.mqtt = {
    connect: function () {
      var h = {}, timer = null, cl;
      function emit(ev, a, b) { (h[ev] || []).forEach(function (f) { f(a, b); }); }
      function msg(topic, text) {
        emit('message', topic, { toString: function () { return text; } });
      }
      async function poll() {
        try {
          var r = await fetch('/api/state');
          var t = await r.text();
          msg(T_STATUS, 'online');
          msg(T_STATE, t);
        } catch (e) {
          msg(T_STATUS, 'offline');
        }
      }
      cl = {
        connected: false,
        on: function (e, f) { (h[e] = h[e] || []).push(f); return cl; },
        subscribe: function () {
          poll();
          if (!timer) timer = setInterval(poll, 1000);
          return cl;
        },
        publish: function (topic, payload) {
          if (topic === T_CMD) fetch('/api/cmd?' + payload).then(poll);
          return cl;
        },
        end: function () {
          if (timer) { clearInterval(timer); timer = null; }
          cl.connected = false; emit('close'); return cl;
        }
      };
      cl._emit = emit;
      setTimeout(function () { cl.connected = true; emit('connect'); }, 300);
      return cl;
    }
  };

  // Thanh dong vai cam bien, de thay logic tu dong phan ung.
  // Bo that khong co thanh nay.
  document.addEventListener('DOMContentLoaded', function () {
    var st = {};
    var bar = document.createElement('div');
    bar.style.cssText = 'background:#854f0b;color:#fff;padding:12px 14px;border-radius:12px;' +
      'margin:0 auto 14px;max-width:560px;font-size:13.5px;line-height:1.6';
    bar.innerHTML =
      '<b>ESP32 giả lập — thanh này đóng vai cảm biến, bo thật không có</b>' +
      '<div style="margin-top:8px;display:flex;gap:6px;flex-wrap:wrap">' +
      ['sLow', 'sFull', 'sFlow'].map(function (id) {
        return '<button id="' + id + '" style="flex:1;min-width:118px;padding:8px;' +
          'border-radius:7px;border:1px solid rgba(255,255,255,.45);' +
          'background:rgba(255,255,255,.15);color:#fff;font:inherit;cursor:pointer"></button>';
      }).join('') +
      '</div>';
    document.body.insertBefore(bar, document.body.firstChild);

    function paint() {
      document.getElementById('sLow').textContent  = st.aboveLow ? 'Bồn: còn nước' : 'Bồn: ĐÃ CẠN';
      document.getElementById('sFull').textContent = st.isFull   ? 'Phao đầy: ĐÃ ĐẦY' : 'Phao đầy: chưa';
      document.getElementById('sFlow').textContent = (st.fTank > 0 || st.flowSet !== 0)
        ? 'Có nước: 20 L/ph' : 'KHÔNG có nước';
    }
    async function refresh() {
      try {
        var r = await fetch('/api/state');
        var s = await r.json();
        st.aboveLow = s.aboveLow; st.isFull = s.isFull;
        paint();
      } catch (e) {}
    }
    async function send(q) { await fetch('/api/cmd?' + q); refresh(); }

    document.getElementById('sLow').onclick  = function () { send('simAboveLow=' + (st.aboveLow ? 0 : 1)); };
    document.getElementById('sFull').onclick = function () { send('simIsFull='   + (st.isFull   ? 0 : 1)); };
    document.getElementById('sFlow').onclick = function () {
      st.flowSet = (st.flowSet === 0) ? 20 : 0;
      send('simFlow=' + st.flowSet);
    };
    st.flowSet = 20;
    refresh();
    setInterval(refresh, 1500);
  });
})();
