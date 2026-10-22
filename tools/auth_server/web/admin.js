/* 管理端逻辑：令牌 -> 设备台账 / 用户 / 绑定 / 吊销 / 审计 */
(function () {
  "use strict";

  var A = window.App;
  var KEY = "dso_admin_token";
  var token = sessionStorage.getItem(KEY) || null;
  var auditTimer = null;

  function hdr() {
    return token ? { "X-Admin-Token": token } : {};
  }

  function needToken(msgNode) {
    if (token) return true;
    A.setMsg(msgNode, "请先在上方保存管理员令牌", "err");
    return false;
  }

  function onAuthError(e, msgNode) {
    if (e.status === 401) {
      token = null;
      sessionStorage.removeItem(KEY);
      A.setMsg(msgNode, "令牌无效或已过期，请重新输入", "err");
      return true;
    }
    return false;
  }

  async function loadDevices() {
    var msg = A.el("devicesMsg");
    if (!needToken(msg)) return;
    A.setMsg(msg, "加载中…", "info");
    try {
      var data = await A.api("GET", "/api/admin/devices", undefined, hdr());
      var body = A.el("devicesBody");
      body.textContent = "";
      data.devices.forEach(function (d) {
        var tr = document.createElement("tr");
        A.cell(tr, d.device, "mono");
        var cap = A.cell(tr, "");
        cap.appendChild(A.badge(d.capability,
          d.capability === "provisioned" ? "on" : "warn"));
        A.cell(tr, d.host ? (d.host + ":" + d.port) : "未登记", "mono");
        A.cell(tr, d.bindings);
        body.appendChild(tr);
      });
      A.setMsg(msg, "共 " + data.devices.length + " 台设备", "ok");
    } catch (e) {
      if (!onAuthError(e, msg)) A.setMsg(msg, "加载失败：" + e.code + " — " + e.message, "err");
    }
  }

  async function loadAudit() {
    var msg = A.el("auditMsg");
    if (!needToken(msg)) return;
    try {
      var data = await A.api("GET", "/api/admin/audit?limit=100", undefined, hdr());
      var body = A.el("auditBody");
      body.textContent = "";
      data.events.slice().reverse().forEach(function (ev) {
        var tr = document.createElement("tr");
        A.cell(tr, A.fmtTs(ev.ts), "mono");
        var kind = A.cell(tr, "");
        var cls = ev.event.indexOf("_ok") > 0 || ev.event === "authorize_ok" ? "on"
          : (ev.event.indexOf("failed") >= 0 || ev.event.indexOf("denied") >= 0) ? "off"
            : "";
        kind.appendChild(A.badge(ev.event, cls));
        A.cell(tr, ev.user || "");
        A.cell(tr, ev.device || "", "mono");
        A.cell(tr, ev.lic || "", "mono");
        A.cell(tr, ev.peer || "");
        body.appendChild(tr);
      });
      A.setMsg(msg, "");
    } catch (e) {
      if (!onAuthError(e, msg)) A.setMsg(msg, "加载失败：" + e.code + " — " + e.message, "err");
    }
  }

  async function post(path, body, msgNode, okText) {
    if (!needToken(msgNode)) return;
    A.setMsg(msgNode, "提交中…", "info");
    try {
      await A.api("POST", path, body, hdr());
      A.setMsg(msgNode, okText, "ok");
      await loadDevices();
      await loadAudit();
    } catch (e) {
      if (!onAuthError(e, msgNode)) {
        A.setMsg(msgNode, "失败：" + e.code + " — " + e.message, "err");
      }
    }
  }

  A.el("saveToken").addEventListener("click", function () {
    token = A.el("token").value.trim();
    if (!token) { A.setMsg(A.el("tokenMsg"), "令牌为空", "err"); return; }
    sessionStorage.setItem(KEY, token);
    A.setMsg(A.el("tokenMsg"), "已保存（仅本标签页有效）", "ok");
    loadDevices();
    loadAudit();
  });

  A.el("loadDevices").addEventListener("click", loadDevices);
  A.el("loadAudit").addEventListener("click", loadAudit);

  A.el("addUser").addEventListener("click", function () {
    post("/api/admin/users",
         { user: A.el("newUser").value.trim(), password: A.el("newPass").value },
         A.el("userMsg"), "用户已创建");
  });

  A.el("addDevice").addEventListener("click", function () {
    var port = parseInt(A.el("devPort").value, 10);
    post("/api/admin/devices", {
      device: A.el("devId").value.trim(),
      pubkey: A.el("devPub").value.trim(),
      host: A.el("devHost").value.trim(),
      port: isNaN(port) ? 5005 : port,
      capability: A.el("devCap").value
    }, A.el("deviceMsg"), "设备已登记");
  });

  A.el("addBind").addEventListener("click", function () {
    var days = parseInt(A.el("bindDays").value, 10);
    post("/api/admin/bindings", {
      user: A.el("bindUser").value.trim(),
      device: A.el("bindDev").value.trim(),
      days: isNaN(days) ? 30 : days
    }, A.el("bindMsg"), "绑定已创建");
  });

  A.el("revokeJtiBtn").addEventListener("click", function () {
    post("/api/admin/revoke", { jti: A.el("revokeJti").value.trim() },
         A.el("revokeMsg"), "License 已吊销");
  });

  A.el("revokeDevBtn").addEventListener("click", function () {
    post("/api/admin/revoke", { device: A.el("revokeDev").value.trim() },
         A.el("revokeMsg"), "设备已吊销（不再签发）");
  });

  A.el("autoAudit").addEventListener("change", function (ev) {
    if (auditTimer) { clearInterval(auditTimer); auditTimer = null; }
    if (ev.target.checked) auditTimer = setInterval(loadAudit, 10000);
  });

  if (token) {
    A.el("token").value = token;
    A.setMsg(A.el("tokenMsg"), "已从本标签页恢复令牌", "info");
    loadDevices();
    loadAudit();
    if (A.el("autoAudit").checked) auditTimer = setInterval(loadAudit, 10000);
  }
}());
