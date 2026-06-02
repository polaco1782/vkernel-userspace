Vendored text fonts for `vkgui`.

Included families:
- `Default`: Dear ImGui built-in font, no external asset
- `DejaVu Sans`: sourced from `/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf`
- `Liberation Sans`: sourced from `/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf`
- `Liberation Mono`: sourced from `/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf`

Reference docs on this machine:
- DejaVu README: `/usr/share/doc/dejavu-sans-fonts/README.md`
- Liberation README: `/usr/share/doc/liberation-sans-fonts/README.md`

The generated `*.h` files in this folder were produced with `xxd -i` so `vkgui`
can embed the fonts directly and switch families at runtime without depending
on guest filesystem font files.
