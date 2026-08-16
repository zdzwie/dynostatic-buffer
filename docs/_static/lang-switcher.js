// Language switcher for the GitHub Pages build.
//
// The CI workflow deploys the English site at the artifact root and nests the
// Polish translation directly under ``pl/`` (see .github/workflows/ci.yml).
// This inserts a button into the sidebar that toggles between the two.
//
// Read the Docs already provides its own language flyout and uses a different
// URL layout (``/en/<version>/`` vs ``/pl/<version>/``), so the button is
// skipped there to avoid a broken or duplicate switcher.
(function () {
  "use strict";

  function onReadTheDocs() {
    var host = window.location.hostname;
    return (
      typeof window.READTHEDOCS_DATA !== "undefined" ||
      /\.readthedocs\.(io|org|build)$/.test(host)
    );
  }

  // True when the current page is served from the Polish sub-site (``pl/``).
  function isPolish() {
    return /\/pl\/[^/]*$/.test(window.location.pathname);
  }

  // File name of the current page, defaulting to the directory index.
  function currentFile() {
    var path = window.location.pathname;
    var file = path.substring(path.lastIndexOf("/") + 1);
    return file || "index.html";
  }

  function build() {
    if (onReadTheDocs()) {
      return;
    }

    var search = document.querySelector(".wy-side-nav-search");
    if (!search) {
      return;
    }

    var polish = isPolish();
    var file = currentFile() + window.location.hash;
    // Relative links keep working under any base path (e.g. GitHub Pages
    // project sites) because the Polish site lives directly under the
    // English one: ``pl/<file>`` from English, ``../<file>`` from Polish.
    var target = polish ? "../" + file : "pl/" + file;
    var label = polish ? "English" : "Polski";

    var link = document.createElement("a");
    link.className = "lang-switcher";
    link.href = target;
    link.textContent = label;
    link.setAttribute(
      "aria-label",
      polish ? "Switch to English" : "Switch to Polish"
    );

    search.appendChild(link);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", build);
  } else {
    build();
  }
})();
