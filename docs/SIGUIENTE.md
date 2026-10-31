# Cómo seguir (31/10/2026)

## Hecho en esta tanda
- reddit r/PS3: carga en ~73 s (antes 183 s) y sin JS trabado.
  - querySelector más rápido: búsqueda por clase en C (`ns.byClass`), atajo para `:scope >` y sin regex ni hermanos calculados de más.
  - Para diagnosticar: el watchdog ahora loguea la pila de JS y la consola avisa los `qsa lento`.
- TextEncoder sin Array (antes daba "out of memory"); Popover API; shadow DOM simple; iteradores arreglados. js-test da 40/40.
- Fondos rgba con alfa real: patch-alpha.py, en el plotter del framebuffer. Antes los botones de MercadoLibre salían azul sólido y sin texto.

## Pendiente, en orden
1. **MercadoLibre** con el arreglo de alfa: los botones ya no salen azul sólido.
   - Ahora la lista "Productos más buscados" sale amontonada (los links sin separación) y se encima con el menú de categorías.
   - Revisar flex-wrap/gap en esa lista y el header (la caja blanca es el buscador, sale sin borde).
   - Captura: kit-ps3/capturas-test/29-meli.jpg (antes del arreglo).
2. Volver a probar reddit, GitHub y Wikipedia con el alfa, para descartar regresiones (overlays rgba(0,0,0,.x)).
3. reddit:
   - A veces se queda sin memoria en JSON.stringify (límite de 96 MB). El log "EonSurf: JS heap ..." dice cuánto ocupa el heap; evaluar subir el límite a 128 MB.
   - Falta `validity.badInput` en los inputs (ValidityState).
   - El menú de orden (Best/Hot/New) se ve abierto: es un dropdown de faceplate, no un popover.
   - La rueda del mouse no scrollea en RPCS3 (usar el teclado).
4. YouTube, Infobae y BBC (el congelamiento T6: con el watchdog con pila ahora se ve dónde).
5. X: avatar enorme y timeline corrida a la derecha.
6. Worker (T8), velocidad (T9), fuente CJK (T10).
7. Re-test general, actualizar docs/PRUEBAS-SITIOS.md y PLAN-WEB.md, PKG nuevo.

## Cómo probar
- Build: `MSYS_NO_PATHCONV=1 wsl -d Ubuntu-24.04 -u root -- env EONSURF_VERBOSE=1 bash /mnt/j/EonPS3/kit-ps3/sync-build.sh` (`--fresh` si se cambian parches de css/*.py).
- Abrir: `J:\EonPS3\kit-ps3\abrir-sitio.ps1 -Url ... -Espera 120`.
  - Después de un build, RPCS3 recompila: usar -Espera 110 o más.
  - js-test: `file:///dev_hdd0/game/EONS00001/USRDIR/startpage/js-test.html`.
- Cerrar y medir: `medidas.ps1 -Nombre x` (el TTY queda en rpcs3-prueba/log/TTY.log).
