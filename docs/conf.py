# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sphinx configuration file for the Chromium EC project."""

# Disable some linters that conflict with Sphinx
# pylint: disable=C0103
# pylint: disable=W0622

project = "Chromium EC"
author = "Google"
copyright = "2025, The ChromiumOS Authors"
release = "0.0.1"
exclude_patterns = [
    "**/*bazel*",
]
extensions = [
    "breathe",
    "sphinxcontrib.mermaid",
    "sphinx_design",
    "sphinx_reredirects",
    "sphinx_sitemap",
    "sphinx_copybutton",  # Copy-to-clipboard button on code blocks
]
html_theme = "pydata_sphinx_theme"

html_theme_options = {
    "logo": {
        "image_light": "_static/img/logo_300px.png",
        "image_dark": "_static/img/logo_300px.png",
        "text": "Chromium EC",
    },
    "show_prev_next": False,
    # https://pydata-sphinx-theme.readthedocs.io/en/stable/user_guide/layout.html#configure-the-navbar-center-alignment
    "navbar_align": "right",
}

templates_path = ["layout"]

html_static_path = ["_static"]

html_extra_path = ["doxygen"]

html_sidebars = {
    "index": [],
    "getting_started": [],
}

html_css_files = [
    "css/cros_ec.css",
    "css/dark_cros_ec.css",
    # FontAwesome for mermaid and sphinx-design
    "https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css",
]

html_favicon = "_static/img/logo_64px.png"

html_js_files = [
    # Do not list cros_ec.js here. This will cause it to get loaded in <head>.
    # To improve load performance we modified //docs/layout/layout.html
    # to load cros_ec.js at the end of <body> instead.
]

mermaid_init_js = """
mermaid.initialize({
  // Mermaid is manually started in //docs/_static/js/cros_ec.js.
  startOnLoad: false,
  // sequenceDiagram Note text alignment
  noteAlign: "left",
  // Set mermaid theme to the current furo theme
  theme: localStorage.getItem("theme") == "dark" ? "dark" : "default"
});
"""

mermaid_version = "11.7.0"

breathe_projects = {
    "Chromium EC": "./_doxygen/xml/",
}
breathe_default_project = "Chromium EC"
breathe_default_members = ("briefdescription", "func", "members")
breathe_debug_trace_directives = False
# (b/295023422) Disable the inaccurate `#include` statements that are generated
# when `doxygennamespace` is used.
breathe_show_include = False

copybutton_prompt_text = r"\(\w+\)\$ "
copybutton_prompt_is_regexp = True


def env_get_outdated():
    """
    Problem: CSS files aren't copied after modifying them. Solution:
    https://github.com/sphinx-doc/sphinx/issues/2090#issuecomment-572902572
    """
    return ["index"]


def setup(app):
    """Sphinx entry point"""
    app.add_css_file("css/cros_ec.css")
    app.add_css_file("css/dark_cros_ec.css")
    app.connect("env-get-outdated", env_get_outdated)
