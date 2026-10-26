(function () {
  var k = "pc-control-theme";
  var v = localStorage.getItem(k);
  if (v === "light" || v === "dark" || v === "system") {
    document.documentElement.setAttribute("data-theme", v);
  } else {
    document.documentElement.setAttribute("data-theme", "system");
  }
})();
