(function () {
  var STORAGE_KEY = "pc-control-theme";

  function normalize(theme) {
    if (theme === "light" || theme === "dark" || theme === "system")
      return theme;
    return "system";
  }

  function stored() {
    return normalize(localStorage.getItem(STORAGE_KEY));
  }

  function apply(theme) {
    theme = normalize(theme);
    document.documentElement.setAttribute("data-theme", theme);
    localStorage.setItem(STORAGE_KEY, theme);
  }

  function syncRadios(theme) {
    theme = normalize(theme);
    document
      .querySelectorAll('input[name="theme_mode"]')
      .forEach(function (el) {
        el.checked = el.value === theme;
      });
  }

  document.querySelectorAll('input[name="theme_mode"]').forEach(function (el) {
    el.addEventListener("change", function () {
      if (!el.checked) return;
      apply(el.value);
    });
  });

  window.pcThemeApply = apply;
  window.pcThemeSyncRadios = syncRadios;
  window.pcThemeStored = stored;
})();
