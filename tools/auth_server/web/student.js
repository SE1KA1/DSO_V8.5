/* 学生端逻辑：登录 -> 选设备 -> 授权 / 续期 / 查看设备状态 */
(function () {
  "use strict";

  var A = window.App;
  var KEY = "dso_auth_sid";
  var sid = sessionStorage.getItem(KEY) || null;

  function showLoggedIn(on) {
    A.el("loginCard").classList.toggle("hide", on);
    A.el("authCard").classList.toggle("hide", !on);
    if (!on) {
      A.el("resultCard").classList.add("hide");
      A.el("deviceCard").classList.add("hide");
      A.el("historyCard").classList.add("hide");
    }
  }

  function renderKv(dl, pairs) {
    dl.textContent = "";
    pairs.forEach(function (pair) {
      var dt = document.createElement("dt");
      var dd = document.createElement("dd");
      dt.textContent = pair[0];
      dd.textContent = (pair[1] === null || pair[1] === undefined) ? "-" : String(pair[1]);
      dl.appendChild(dt);
      dl.appendChild(dd);
    });
  }

  function activeDevice() {
    var sel = A.el("device");
    return sel && sel.value ? sel.value : null;
  }

  async function loadDevices() {
    var msg = A.el("authMsg");
    A.setMsg(msg, "正在获取设备列表…", "info");
    try {
      var data = await A.api("GET", "/api/devices", undefined, { "X-Session": sid });
      var sel = A.el("device");
      sel.textContent = "";
      data.devices.forEach(function (d) {
        var opt = document.createElement("option");
        opt.value = d.device;
        opt.textContent = d.device + (d.capability === "identity_only"
          ? "（仅身份）" : "");
        sel.appendChild(opt);
      });
      A.setMsg(msg, data.devices.length ? "" : "没有可用设备", data.devices.length ? "ok" : "info");
    } catch (e) {
      if (e.code === "session_expired" || e.status === 401) {
        sessionStorage.removeItem(KEY);
        sid = null;
        showLoggedIn(false);
        A.setMsg(A.el("loginMsg"), "会话已过期，请重新登录（" + e.code + "）", "err");
        return;
      }
      A.setMsg(msg, "获取设备失败：" + e.code + " — " + e.message, "err");
    }
  }

  async function authorize(renew) {
    var dev = activeDevice();
    var msg = A.el("authMsg");
    if (!dev) { A.setMsg(msg, "请先选择设备", "err"); return; }
    var btn = renew ? A.el("renewBtn") : A.el("authorizeBtn");
    var label = btn.textContent;
    btn.disabled = true;
    A.el("authorizeBtn").disabled = true;
    btn.textContent = "正在驱动设备…";
    A.setMsg(msg, "正在请求设备签名并下发 License…", "info");
    try {
      var data = await A.api("POST", "/api/authorize", { sid: sid, device: dev });
      A.setMsg(msg, renew ? "续期成功" : "授权成功，设备已收到 License", "ok");
      renderKv(A.el("resultKv"), [
        ["License", data.lic],
        ["用户", data.user],
        ["设备", data.dev],
        ["生效", A.fmtOffset(data.nb)],
        ["到期", A.fmtOffset(data.na)],
        ["权限", data.perm === 1 ? "FPGA（1）" : String(data.perm)]
      ]);
      A.el("resultCard").classList.remove("hide");
      await showDeviceStatus();
      await loadHistory();
    } catch (e) {
      A.setMsg(msg, "失败：" + e.code + " — " + e.message, "err");
    } finally {
      btn.disabled = false;
      A.el("authorizeBtn").disabled = false;
      btn.textContent = label;
    }
  }

  async function loadHistory() {
    var msg = A.el("historyMsg");
    try {
      var data = await A.api("GET", "/api/history?limit=50", undefined,
                             { "X-Session": sid });
      var body = A.el("historyBody");
      body.textContent = "";
      data.history.forEach(function (h) {
        var tr = document.createElement("tr");
        A.cell(tr, h.lic, "mono");
        A.cell(tr, h.dev, "mono");
        A.cell(tr, A.fmtOffset(h.nb));
        A.cell(tr, A.fmtOffset(h.na));
        var st = A.cell(tr, "");
        st.appendChild(A.badge(h.revoked ? "已吊销" : "有效",
                               h.revoked ? "off" : "on"));
        body.appendChild(tr);
      });
      A.setMsg(msg, data.history.length ? "" : "还没有授权记录", "info");
      A.el("historyCard").classList.remove("hide");
    } catch (e) {
      A.setMsg(msg, "读取历史失败：" + e.code + " — " + e.message, "err");
    }
  }

  async function showDeviceStatus() {
    var dev = activeDevice();
    if (!dev) return;
    var msg = A.el("authMsg");
    try {
      var data = await A.api("GET", "/api/device/" + encodeURIComponent(dev) + "/license",
                             undefined, { "X-Session": sid });
      var state = A.el("deviceState");
      state.textContent = "";
      state.appendChild(A.badge(data.login_state === "AUTHORIZED" ? "已授权" : "未授权",
                                data.login_state === "AUTHORIZED" ? "on" : "off"));
      var lic = data.license;
      renderKv(A.el("deviceKv"), lic ? [
        ["License", lic.lic],
        ["用户", lic.user],
        ["设备", lic.dev],
        ["生效", A.fmtOffset(lic.nb)],
        ["到期", A.fmtOffset(lic.na)],
        ["权限", lic.perm]
      ] : [["状态", data.login_state]]);
      A.el("deviceCard").classList.remove("hide");
    } catch (e) {
      A.setMsg(msg, "读取设备状态失败：" + e.code + " — " + e.message, "err");
    }
  }

  A.el("loginForm").addEventListener("submit", async function (ev) {
    ev.preventDefault();
    var msg = A.el("loginMsg");
    var btn = A.el("loginBtn");
    btn.disabled = true;
    A.setMsg(msg, "登录中…", "info");
    try {
      var data = await A.api("POST", "/api/session", {
        user: A.el("user").value.trim(),
        password: A.el("password").value
      });
      sid = data.sid;
      sessionStorage.setItem(KEY, sid);
      A.setMsg(msg, "");
      A.el("password").value = "";
      showLoggedIn(true);
      await loadDevices();
      await loadHistory();
    } catch (e) {
      A.setMsg(msg, "登录失败：" + e.code + " — " + e.message, "err");
    } finally {
      btn.disabled = false;
    }
  });

  A.el("refreshBtn").addEventListener("click", loadDevices);
  A.el("authorizeBtn").addEventListener("click", function () { authorize(false); });
  A.el("renewBtn").addEventListener("click", function () { authorize(true); });
  A.el("statusBtn").addEventListener("click", showDeviceStatus);
  A.el("logoutBtn").addEventListener("click", async function () {
    try { await A.api("POST", "/api/session/logout", { sid: sid }); } catch (e) { /* ignore */ }
    sessionStorage.removeItem(KEY);
    sid = null;
    showLoggedIn(false);
    A.setMsg(A.el("loginMsg"), "已退出登录", "info");
  });

  if (sid) {
    showLoggedIn(true);
    loadDevices();
    loadHistory();
  } else {
    showLoggedIn(false);
  }
}());
