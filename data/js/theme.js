(function () {
  var STORAGE_KEY = "pc-control-theme";

  function apply(theme) {
    if (theme !== "light" && theme !== "dark" && theme !== "system") return;
    document.documentElement.setAttribute("data-theme", theme);
    localStorage.setItem(STORAGE_KEY, theme);
  }

  function syncRadios(theme) {
    document
      .querySelectorAll('input[name="theme_mode"]')
      .forEach(function (el) {
        el.checked = el.value === theme;
      });
  }

  document.querySelectorAll('input[name="theme_mode"]').forEach(function (el) {
    el.addEventListener("change", function () {
      if (!el.checked) return;
      fetch("/api/theme", {
        method: "POST",
        credentials: "same-origin",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "theme_mode=" + encodeURIComponent(el.value),
      })
        .then(function (r) {
          return r.json();
        })
        .then(function (j) {
          if (j.ok) apply(el.value);
        })
        .catch(function () {});
    });
  });

  window.pcThemeApply = apply;
  window.pcThemeSyncRadios = syncRadios;
})();
