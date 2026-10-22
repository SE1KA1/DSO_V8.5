/* 授权服务前端公共库：同源 fetch 封装 + 展示工具 */
(function () {
  "use strict";

  var EPOCH_BASE = 1577836800;      /* licenses use seconds since 2020-01-01 */

  async function api(method, path, body, headers) {
    var opts = { method: method, credentials: "same-origin", headers: {} };
    var key;
    for (key in (headers || {})) {
      if (Object.prototype.hasOwnProperty.call(headers, key)) opts.headers[key] = headers[key];
    }
    if (body !== undefined) {
      opts.headers["Content-Type"] = "application/json";
      opts.body = JSON.stringify(body);
    }
    var res;
    try {
      res = await fetch(path, opts);
    } catch (e) {
      var netErr = new Error("无法连接服务");
      netErr.code = "network";
      throw netErr;
    }
    var data = null;
    try { data = await res.json(); } catch (e) { data = null; }
    if (!res.ok || !data || data.ok === false) {
      var err = new Error((data && (data.error || data.code)) || ("HTTP " + res.status));
      err.code = (data && data.code) || ("http_" + res.status);
      err.status = res.status;
      err.detail = data && data.detail;
      throw err;
    }
    return data;
  }

  function fmtOffset(off) {
    if (off === null || off === undefined) return "-";
    var d = new Date((EPOCH_BASE + Number(off)) * 1000);
    return d.toLocaleString();
  }

  function fmtTs(ts) {
    if (!ts) return "-";
    return new Date(Number(ts) * 1000).toLocaleTimeString();
  }

  function el(id) { return document.getElementById(id); }

  function setMsg(node, text, kind) {
    if (!node) return;
    node.textContent = text || "";
    node.className = "msg" + (kind ? " " + kind : "");
  }

  function badge(text, kind) {
    var span = document.createElement("span");
    span.className = "badge" + (kind ? " " + kind : "");
    span.textContent = text;
    return span;
  }

  function cell(row, text, cls) {
    var td = document.createElement("td");
    if (cls) td.className = cls;
    td.textContent = (text === null || text === undefined) ? "-" : String(text);
    row.appendChild(td);
    return td;
  }

  function jsonText(obj) { return JSON.stringify(obj); }

  window.App = { api: api, fmtOffset: fmtOffset, fmtTs: fmtTs, el: el,
                 setMsg: setMsg, badge: badge, cell: cell, jsonText: jsonText };
}());
