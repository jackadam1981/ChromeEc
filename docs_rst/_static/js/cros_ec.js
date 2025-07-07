/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

window.addEventListener('DOMContentLoaded', () => {
    // Manually control when Mermaid diagrams render to prevent scrolling
    // issues.
    // Context: https://pigweed.dev/docs/style_guide.html#site-nav-scrolling
    if (window.mermaid) {
        // https://mermaid.js.org/config/usage.html#using-mermaid-run
        window.mermaid.run();
    }
});

const init_mermaid = (mutationsList) => {
    console.log("something changed");
    const theme_changed = mutationsList.some(mutation => {
        console.log(mutation)
        console.log(mutation.attributeName);
        return mutation.type === 'attributes' &&
            mutation.attributeName === 'data-theme';
    });
    if (!theme_changed) {
        return;
    }
    console.log("re-initializing mermaid...");

    let graphs = document.querySelectorAll(".mermaid");
    [...graphs].forEach((element) => {
        if (!element.hasAttribute("data-source")) {
            element.setAttribute("data-source", element.innerHTML.trim());
        }
        const source = element.getAttribute("data-source");
        element.innerHTML = source;
        element.removeAttribute("data-processed");
    });

    mermaid.initialize({
        // Mermaid is manually started in //docs/_static/js/cros_ec.js.
        startOnLoad: false,
        // sequenceDiagram Note text alignment
        noteAlign: "left",
        // Set mermaid theme to the current furo theme
        theme: localStorage.getItem("theme") == "dark" ? "dark" : "default"
    });
    mermaid.run();
}

let theme_observer = new MutationObserver(init_mermaid);
let body = document.getElementsByTagName("html")[0];
theme_observer.observe(body, { attributes: true });
window.theme_observer = theme_observer;
