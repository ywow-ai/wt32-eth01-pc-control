(function () {
  var EMPTY = "()";

  function $(id) {
    return document.getElementById(id);
  }

  function showMsg(el, text, ok) {
    if (!el) return;
    el.textContent = text;
    el.classList.remove("hidden", "msg-ok", "msg-err");
    el.classList.add(ok ? "msg-ok" : "msg-err");
  }

  function hideMsg(el) {
    if (el) el.classList.add("hidden");
  }

  function disp(v) {
    if (v === undefined || v === null || v === "") return EMPTY;
    return String(v);
  }

  async function api(path, opts) {
    var res = await fetch(
      path,
      Object.assign({ credentials: "same-origin" }, opts || {}),
    );
    var ct = res.headers.get("content-type") || "";
    var body = ct.indexOf("json") >= 0 ? await res.json() : await res.text();
    if (!res.ok) {
      var err = new Error(body && body.error ? body.error : "request failed");
      err.body = body;
      throw err;
    }
    return body;
  }

  /** ESP32 WebServer reads application/x-www-form-urlencoded, not multipart FormData. */
  function formPost(fd) {
    return {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams(fd).toString(),
    };
  }

  function formatUptime(sec) {
    var h = Math.floor(sec / 3600);
    var m = Math.floor((sec % 3600) / 60);
    var s = sec % 60;
    return (
      String(h).padStart(2, "0") +
      ":" +
      String(m).padStart(2, "0") +
      ":" +
      String(s).padStart(2, "0")
    );
  }

  function ipv4Ok(s) {
    if (!s) return true;
    var p = s.split(".");
    if (p.length !== 4) return false;
    for (var i = 0; i < 4; i++) {
      var n = parseInt(p[i], 10);
      if (isNaN(n) || n < 0 || n > 255 || String(n) !== p[i]) return false;
    }
    return true;
  }

  async function refreshStatus() {
    var d = await api("/api/status");
    $("topbar-ip").textContent = disp(d.ip);
    $("st-link").textContent = d.link ? d.link_text || "Up" : EMPTY;
    $("st-active") && ($("st-active").textContent = disp(d.active_if));
    $("st-ip").textContent = disp(d.ip);
    $("st-eth-ip") && ($("st-eth-ip").textContent = disp(d.eth_ip));
    $("st-wifi-ip") && ($("st-wifi-ip").textContent = disp(d.wifi_ip));
    $("st-gw").textContent = disp(d.gateway);
    $("st-mask").textContent = disp(d.subnet);
    $("st-dns").textContent = disp(d.dns);
    $("st-mac").textContent = disp(d.mac);
    $("st-host").textContent = disp(d.hostname);
    $("st-chip").textContent = disp(d.chip);
    $("st-uptime").textContent =
      d.uptime_sec != null ? formatUptime(d.uptime_sec) : EMPTY;
    $("st-heap").textContent = d.free_heap != null ? d.free_heap + " B" : EMPTY;
  }

  function fillNetworkConfig(c) {
    if (!c) return;
    var netLink = c.net_link || "auto";
    document.querySelectorAll('input[name="net_link"]').forEach(function (el) {
      el.checked = el.value === netLink;
    });
    var ipMode = c.ip_mode || "dhcp";
    document.querySelectorAll('input[name="ip_mode"]').forEach(function (el) {
      el.checked = el.value === ipMode;
    });
    $("wifi_ssid") && ($("wifi_ssid").value = c.wifi_ssid || "");
    $("wifi_pass") && ($("wifi_pass").value = "");
    $("ip_addr") && ($("ip_addr").value = c.ip_addr || "");
    $("subnet") && ($("subnet").value = c.subnet || "");
    $("gateway") && ($("gateway").value = c.gateway || "");
    $("dns1") && ($("dns1").value = c.dns1 || "");
    $("dns2") && ($("dns2").value = c.dns2 || "");
  }

  function fillSettingsConfig(c) {
    if (!c) return;
    var theme = window.pcThemeStored ? window.pcThemeStored() : "system";
    if (window.pcThemeApply) window.pcThemeApply(theme);
    if (window.pcThemeSyncRadios) window.pcThemeSyncRadios(theme);

    $("pusher_chan").value = c.pusher_chan || "";
    $("ws_host").value = c.ws_host || "";
    $("ws_port").value = c.ws_port || "";
    $("ws_path").value = c.ws_path || "";
    $("pusher_key").value = c.pusher_key || "";
    $("pusher_cluster").value = c.pusher_cluster || "";

    $("discord_enabled").checked = !!c.discord_enabled;
    $("discord_url").value = c.discord_url || "";
    $("discord_username").value = c.discord_username || "";
    $("discord_events").value = c.discord_events || "";

    var defCpu = 160;
    var defDelay = 5;
    if ($("cpu_mhz")) {
      var cpu = parseInt(c.cpu_mhz, 10);
      $("cpu_mhz").value =
        [80, 160, 240].indexOf(cpu) >= 0 ? String(cpu) : String(defCpu);
    }
    if ($("loop_delay_ms")) {
      var d = parseInt(c.loop_delay_ms, 10);
      $("loop_delay_ms").value =
        !isNaN(d) && d >= 0 && d <= 100 ? String(d) : String(defDelay);
    }
  }

  async function reloadConfigFromDevice() {
    var c = await api("/api/config");
    fillNetworkConfig(c);
    fillSettingsConfig(c);
    return c;
  }

  /** First open: pull NVS form values + dashboard status once (no polling). */
  async function loadAllFromDevice() {
    await reloadConfigFromDevice();
    await refreshStatus();
  }

  function wireLogin() {
    var form = $("form-login");
    if (!form) return;
    form.addEventListener("submit", async function (e) {
      e.preventDefault();
      hideMsg($("login-msg"));
      var fd = new FormData(form);
      try {
        await api("/api/login", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: new URLSearchParams(fd).toString(),
        });
        location.href = "/";
      } catch (err) {
        showMsg(
          $("login-msg"),
          (err.body && err.body.error) || "Login failed",
          false,
        );
      }
    });
  }

  function wireIndex() {
    $("nav-logout") &&
      $("nav-logout").addEventListener("click", async function (e) {
        e.preventDefault();
        await api("/api/logout", { method: "POST" });
        location.href = "/login.html";
      });

    $("form-password") &&
      $("form-password").addEventListener("submit", async function (e) {
        e.preventDefault();
        hideMsg($("pw-msg"));
        var fd = new FormData(e.target);
        if (fd.get("newpass").length < 8) {
          showMsg($("pw-msg"), "Min 8 characters", false);
          return;
        }
        if (fd.get("newpass").length > 64) {
          showMsg($("pw-msg"), "Max 64 characters", false);
          return;
        }
        if (fd.get("newpass") !== fd.get("newpass2")) {
          showMsg($("pw-msg"), "Passwords do not match", false);
          return;
        }
        try {
          await api("/api/password", formPost(fd));
          await reloadConfigFromDevice();
          showMsg($("pw-msg"), "Saved", true);
          e.target.reset();
        } catch (err) {
          showMsg(
            $("pw-msg"),
            (err.body && err.body.error) || "Save failed",
            false,
          );
        }
      });

    $("btn-dashboard-refresh") &&
      $("btn-dashboard-refresh").addEventListener("click", async function () {
        hideMsg($("dash-msg"));
        try {
          await refreshStatus();
          showMsg($("dash-msg"), "Updated", true);
        } catch (err) {
          showMsg(
            $("dash-msg"),
            (err.body && err.body.error) || "Refresh failed",
            false,
          );
        }
      });

    $("form-network") &&
      $("form-network").addEventListener("submit", async function (e) {
        e.preventDefault();
        hideMsg($("net-msg"));
        var fd = new FormData(e.target);
        var link = fd.get("net_link") || "auto";
        if (link === "wifi" && !String(fd.get("wifi_ssid") || "").trim()) {
          showMsg($("net-msg"), "Wi-Fi SSID required for Wi-Fi only", false);
          return;
        }
        if (fd.get("ip_mode") === "static") {
          if (
            !ipv4Ok(fd.get("ip_addr")) ||
            !ipv4Ok(fd.get("subnet")) ||
            !ipv4Ok(fd.get("gateway"))
          ) {
            showMsg($("net-msg"), "Invalid IPv4", false);
            return;
          }
          if (!ipv4Ok(fd.get("dns1")) || !ipv4Ok(fd.get("dns2"))) {
            showMsg($("net-msg"), "Invalid DNS", false);
            return;
          }
        }
        try {
          await api("/api/network", formPost(fd));
          showMsg($("net-msg"), "Saved, rebooting...", true);
        } catch (err) {
          showMsg(
            $("net-msg"),
            (err.body && err.body.error) || "Save failed",
            false,
          );
        }
      });

    $("btn-dhcp-renew") &&
      $("btn-dhcp-renew").addEventListener("click", async function () {
        hideMsg($("net-msg"));
        try {
          await api("/api/network/renew", { method: "POST" });
          showMsg($("net-msg"), "Renew requested", true);
        } catch (err) {
          showMsg(
            $("net-msg"),
            (err.body && err.body.error) || "Failed",
            false,
          );
        }
      });

    $("form-pusher") &&
      $("form-pusher").addEventListener("submit", async function (e) {
        e.preventDefault();
        hideMsg($("push-msg"));
        var fd = new FormData(e.target);
        var port = fd.get("ws_port");
        if (port && (parseInt(port, 10) < 1 || parseInt(port, 10) > 65535)) {
          showMsg($("push-msg"), "Invalid port", false);
          return;
        }
        try {
          await api("/api/pusher", formPost(fd));
          await reloadConfigFromDevice();
          showMsg($("push-msg"), "Saved", true);
        } catch (err) {
          showMsg(
            $("push-msg"),
            (err.body && err.body.error) || "Save failed",
            false,
          );
        }
      });

    $("form-discord") &&
      $("form-discord").addEventListener("submit", async function (e) {
        e.preventDefault();
        hideMsg($("disc-msg"));
        var fd = new FormData(e.target);
        fd.set("discord_enabled", $("discord_enabled").checked ? "1" : "0");
        if ($("discord_enabled").checked) {
          var u = fd.get("discord_url");
          if (!u || u.indexOf("https://discord") !== 0) {
            showMsg($("disc-msg"), "Invalid webhook URL", false);
            return;
          }
        }
        try {
          await api("/api/discord", formPost(fd));
          await reloadConfigFromDevice();
          showMsg($("disc-msg"), "Saved", true);
        } catch (err) {
          showMsg(
            $("disc-msg"),
            (err.body && err.body.error) || "Save failed",
            false,
          );
        }
      });

    $("btn-discord-test") &&
      $("btn-discord-test").addEventListener("click", async function () {
        hideMsg($("disc-msg"));
        try {
          await api("/api/discord/test", { method: "POST" });
          showMsg($("disc-msg"), "Test queued", true);
        } catch (err) {
          showMsg(
            $("disc-msg"),
            (err.body && err.body.error) || "Test failed",
            false,
          );
        }
      });

    document.querySelectorAll("[data-press]").forEach(function (btn) {
      btn.addEventListener("click", async function () {
        hideMsg($("ctrl-msg"));
        var fd = new FormData();
        fd.append("target", btn.getAttribute("data-press"));
        fd.append("dur", btn.getAttribute("data-dur"));
        try {
          await api("/api/press", formPost(fd));
          showMsg($("ctrl-msg"), "OK", true);
        } catch (err) {
          showMsg(
            $("ctrl-msg"),
            (err.body && err.body.error) || "Failed",
            false,
          );
        }
      });
    });

    $("form-performance") &&
      $("form-performance").addEventListener("submit", async function (e) {
        e.preventDefault();
        hideMsg($("perf-msg"));
        var cpu = parseInt($("cpu_mhz").value, 10);
        var delayMs = parseInt($("loop_delay_ms").value, 10);
        if ([80, 160, 240].indexOf(cpu) < 0) {
          showMsg($("perf-msg"), "CPU MHz must be 80, 160, or 240", false);
          return;
        }
        if (isNaN(delayMs) || delayMs < 0 || delayMs > 100) {
          showMsg($("perf-msg"), "Loop delay must be 0 to 100", false);
          return;
        }
        var fd = new FormData();
        fd.append("cpu_mhz", String(cpu));
        fd.append("loop_delay_ms", String(delayMs));
        try {
          await api("/api/performance", formPost(fd));
          await reloadConfigFromDevice();
          await refreshStatus();
          showMsg($("perf-msg"), "Saved", true);
        } catch (err) {
          showMsg(
            $("perf-msg"),
            (err.body && err.body.error) || "Save failed",
            false,
          );
        }
      });

    $("btn-esp-restart") &&
      $("btn-esp-restart").addEventListener("click", async function () {
        if (!confirm("Restart ESP32 now?")) return;
        fetch("/api/restart", {
          method: "POST",
          credentials: "same-origin",
        }).catch(function () {});
      });

    $("btn-nvs-clear") &&
      $("btn-nvs-clear").addEventListener("click", async function () {
        hideMsg($("nvs-msg"));
        if (
          !confirm(
            "Clear all NVS settings and reboot? Password will reset to default.",
          )
        )
          return;
        var fd = new FormData();
        fd.append("confirm", "yes");
        try {
          await api("/api/nvs/clear", formPost(fd));
          showMsg($("nvs-msg"), "Rebooting...", true);
        } catch (err) {
          showMsg(
            $("nvs-msg"),
            (err.body && err.body.error) || "Clear failed",
            false,
          );
        }
      });
  }

  async function bootIndex() {
    try {
      await api("/api/session");
      await loadAllFromDevice();
    } catch (_) {
      location.href = "/login.html";
    }
  }

  if ($("form-login")) {
    wireLogin();
  } else {
    wireIndex();
    bootIndex();
  }
})();
