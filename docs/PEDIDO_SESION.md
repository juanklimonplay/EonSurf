# Pedido para la sesión en la nube (etapa 1)

Para arrancar: abrir una sesión de Claude Code en este repo y decirle
**"Seguí las instrucciones de docs/PEDIDO_SESION.md"**.

---

Read docs/ESPECIFICACION.md first: it is the full spec (in Spanish). Goal: stage 1 of a native PS3 homebrew
web browser called EonSurf, based on NetSurf, packaged as a .pkg that runs under PS3HEN.

Context: the owner (Argentina, not a developer) has a PS3 Super Slim (CECH-4xxx, to be confirmed) that will run
PS3HEN 3.60 on HFW (Super Slim cannot run CFW). The stock browser is an old WebKit that fails on most modern HTTPS
sites. No homebrew browser exists for PS3 (searched in several languages, Oct 2026). NetSurf ports exist for Switch
(2026, SDL2 + curl + mbedTLS + QuickJS, verified HTTPS on hardware) and 3DS (works in 64-80 MB). ps3libraries
(Oct 2026) already has SDL2, FreeType, cairo, libpng, jpeg, zlib, libxml2, libcurl 7.64.1 and mbedTLS 2.28.10.
Homebrew apps get roughly 200 MB of RAM.

Do, in order, committing often and stopping at a solid checkpoint if time runs out:
1. Toolchain: ps3toolchain + PSL1GHT + ps3libraries (github.com/ps3dev). If the ghcr.io Docker image can't be
   pulled, use prebuilt tarballs or build from source. Record versions in docs/TOOLCHAIN.md.
2. Hello world: minimal PSL1GHT + SDL2 app that draws text and reads the DualShock 3, packaged as a .pkg
   (PARAM.SFO title "EonSurf", placeholder ICON0.PNG). GitHub Actions workflow that builds it and uploads the .pkg.
3. NetSurf, first pass: upstream NetSurf (git.netsurf-browser.org, GPL v2) and its libraries (libparserutils,
   libwapcaplet, hubbub, libcss, libdom, libnsutils, libnspsl, libnsfb, libnsgif, libnsbmp, libsvgtiny...),
   cross-compiled with PSL1GHT. Framebuffer frontend with an SDL2 surface. Network via libcurl + mbedTLS with a
   bundled CA file. Big-endian PowerPC target: watch for endianness bugs.
4. Native Linux headless build of the same tree (framebuffer frontend, ram surface) that renders a URL to a
   screenshot, to test rendering without a PS3. The NetSurf Switch port (github.com/etonedemid/netsurf-switch)
   uses the same stack and has this tooling: use it only as a technical REFERENCE. It declares no license, so do
   not copy its code; reimplement from upstream NetSurf and document anything inspired by it.
5. QuickJS (quickjs-ng preferred) belongs in version 1 because Google Search requires JavaScript since January
   2025. If it doesn't fit, leave it as the documented next step, but verify it compiles for the PS3 target and
   note big-endian issues.
6. PS3 frontend basics: DualShock 3 controls (left stick pointer, right stick scroll, X click, O back, triangle URL
   bar, square menu, L2/R2 zoom), 720p/1080p, files under /dev_hdd0/game/<TITLEID>/USRDIR, home page
   https://www.google.com, mobile user agent by default.
7. UI in Spanish and English (follow the PS3 system language, with a setting to switch).

Keep docs/ESTADO.md updated in simple Spanish: what works, how it was tested, what is untested on real hardware,
next steps. Nothing can be tested on a real PS3 yet; say so. Work on a branch, open a PR into main at each
checkpoint, and report toolchain versions, whether the hello-world .pkg builds, how far the NetSurf build got,
headless screenshots of real sites if achieved, and blockers.
