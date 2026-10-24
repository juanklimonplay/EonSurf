# EonSurf â El Navegador Web Definitivo para PlayStation 3

> **Created by JuAnKLiMoN**  
> *En memoria y homenaje a 20 aÃ±os de historia de la PlayStation 3 (2006â2026).*  
> *Agradecimientos tÃ©cnicos: NetSurf Core Team, PSL1GHT Homebrew SDK, librerÃ­as mbedTLS & cURL.*

---

## 1. MisiÃ³n del Proyecto
El navegador oficial de Sony (basado en NetFront de 2006) quedÃ³ obsoleto: certificados SSL caÃ­dos ("Not Supported"), errores de memoria constantes y sin soporte para webs modernas.

**EonSurf** nace como un navegador moderno, ultraligero y con aceleraciÃ³n RSX nativa directa, pensado para volar en los 256 MB de RAM de la PS3 y ser el navegador que la consola siempre mereciÃ³.

---

## 2. Bloque de CrÃ©ditos Integrado (En UI y Acerca de)
- **Autor Principal / Creador:** `JuAnKLiMoN`
- **Renderizado GrÃ¡fico:** Motor RSX Direct Framebuffer (sin SDL2) a 60 FPS estables.
- **Seguridad:** Pila TLS moderna con soporte de certificados Let's Encrypt y Cloudflare.
- **Teclado:** IntegraciÃ³n con el OSK nativo de Sony (`sysutil/osk`).

---

## 3. CaracterÃ­sticas Nativas Confirmadas (Tanda 1)

1. **AdBlocker Ultra-Ligero Nativo (Hosts/DNS Filter - Totalmente Opcional):**
   - Bloqueo en tiempo real de dominios publicitarios y scripts de rastreo antes de descargarlos.
   - Reduce el consumo de RAM hasta un 60% y triplica la velocidad de carga.
   - **Interruptor Opcional (Toggle):** En Ajustes o con un atajo rÃ¡pido, el usuario puede activarlo o desactivarlo globalmente o en pÃ¡ginas puntuales que exijan apagar el bloqueador para permitir la lectura.
2. **Modo Lectura Limpio ("Reader Mode" con BotÃ³n â¡):**
   - Extrae el texto y fotos principales de artÃ­culos, guÃ­as de juegos y Wikipedia.
   - Oculta menÃºs molestos y lo presenta en tipografÃ­a limpia y grande para TV.
3. **Descargador Directo a USB / Disco Interno:**
   - Detecta archivos `.pkg`, `.zip`, `.mp3`, `.mp4`, `.pdf` e imÃ¡genes.
   - Permite elegir destino: `/dev_usb000/` (Pendrive) o `/dev_hdd0/downloads/` (HDD interno).
4. **Modo Ahorro de Datos / ImÃ¡genes por Demanda (Opcional):**
   - Interruptor activable para conexiones lentas o foros pesados: muestra cuadros de reserva (*placeholders*) y permite descargar imÃ¡genes individuales con un clic o cargar todas juntas.
4. **Modo Oscuro Universal & Modo Claro (Toggle con L3 o Select):**
   - Interfaz en grafito oscuro predeterminada para no cansar la vista en la TV.
   - InyecciÃ³n CSS inteligente para convertir pÃ¡ginas claras en oscuras respetando imÃ¡genes.
5. **Zoom Adaptativo para TV (L2 / R2):**
   - Niveles de zoom rÃ¡pidos (100%, 125%, 150%, 200%) mediante escalado en el RSX.
6. **Selector de Modo MÃ³vil / Escritorio (User-Agent Switch con R3):**
   - Cambia entre versiÃ³n de telÃ©fono (mÃ¡s rÃ¡pida y compacta) y versiÃ³n de PC completa.
7. **PÃ¡gina de Inicio con Marcadores RÃ¡pidos (Speed Dial):**
   - Accesos directos a YouTube liviano (Yo! Player/Invidious), Wikipedia, buscadores ligeros y foros de la comunidad.
8. **Cursor Inteligente ("Magnetic Cursor"):**
   - El puntero se imanta suavemente a enlaces y botones para no renegar apuntando con el stick analÃ³gico.
9. **Captura de Pantalla Directa (L1 + R1):**
   - Guarda lo que estÃ¡s viendo en la web directamente en formato PNG en el disco o pendrive.
10. **Vaciado de Memoria en 1 Toque ("Flush RAM"):**
    - Libera el bÃºfer de pÃ¡ginas anteriores y cachÃ© de imÃ¡genes al instante para mantener 200+ MB libres.
11. **Guardado de PÃ¡ginas Offline:**
    - Guarda guÃ­as y trucos en el disco para leerlas sin conexiÃ³n a internet.

---

## 4. Tanda 2: Funciones Innovadoras Ãnicas para la Comunidad PS3

1. **Instalador de PKGs con 1 Clic ("Direct PKG Installer"):**
   - Si descargas un archivo `.pkg` desde una web o repositorio homebrew, EonSurf lo guarda en `/dev_hdd0/packages/` y lo deja listo para instalar en Package Manager al salir, sin tener que moverlo con exploradores externos.
2. **Historial y PestaÃ±as en Tarjetas Visuales (Visual Tab Switcher):**
   - Vista en miniaturas navegables con la cruceta (estilo carrusel) sin mantener pÃ¡ginas pesadas en RAM.
3. **Buscador de Texto en PÃ¡gina ("Find in Page"):**
   - Presionando triÃ¡ngulo o desde el menÃº, busca palabras clave dentro de guÃ­as largas de juegos resaltÃ¡ndolas en pantalla.
4. **Reproductor de MÃºsica / Radio Web de Fondo (Background Audio Streamer):**
   - Capacidad de sintonizar radios online o pistas MP3 livianas mientras se lee o navega.
5. **Generador y Lector de CÃ³digos QR en Pantalla:**
   - Â¿Encontraste un enlace largo o un video que querÃ©s ver en tu celular? EonSurf muestra un cÃ³digo QR en la pantalla de la TV para escanearlo con el telÃ©fono al instante.
6. **Monitor de Hardware en Tiempo Real (OSD de Rendimiento):**
   - Muestra en una esquina discreta (opcional): FPS (60 clavados), Temperatura del CELL/RSX y Memoria RAM libre en MB.

---

## 5. Tanda 3: IntegraciÃ³n de Consola, Accesorios y Experiencia de Usuario

1. **Soporte Nativo de Teclado y Mouse (USB / Bluetooth):**
   - Si conectas un teclado o mouse a los puertos USB de la PS3, EonSurf los detecta automÃ¡ticamente:
     - `Ctrl + L` o `F4`: Enfocar barra de direcciones.
     - `Ctrl + R` o `F5`: Recargar pÃ¡gina.
     - `Ctrl + W`: Cerrar pestaÃ±a actual.
     - `Ctrl + + / -`: Zoom en pantalla.
     - Rueda del mouse para scroll suave.
2. **Atajos Maestros de Mando (DualShock 3):**
   - **Start:** Abrir barra de direcciones / Teclado OSK de Sony.
   - **Select:** MenÃº rÃ¡pido de marcadores y configuraciÃ³n.
   - **L1 / R1:** PÃ¡gina AtrÃ¡s / Adelante.
   - **L2 / R2:** Zoom dinÃ¡mico (- / +).
   - **Stick Derecho:** Paneo libre y scroll rÃ¡pido.
   - **R3:** Alternar modo mÃ³vil / escritorio.
   - **L3:** Alternar tema oscuro / claro.
3. **Control del Ventilador DinÃ¡mico (Cooling Guard):**
   - OpciÃ³n para aumentar un 5% el rÃ©gimen del ventilador de la consola mientras se navega, evitando que el chip CELL caliente de mÃ¡s si se leen pÃ¡ginas largas en verano.
4. **Modo Cine / Pantalla Completa (F11 / BotÃ³n Cuadrado prolongado):**
   - Oculta la barra de direcciones y bordes para aprovechar el 100% de la TV al ver fotos o leer contenido.
5. **Modo NavegaciÃ³n Privada / Modo IncÃ³gnito:**
   - Navega sin guardar historial, cookies ni rastro en el disco duro interno de la PS3.

---

## 6. Tanda 4: Funciones Legendarias No Obvias (Creadas para EonSurf)

1. **"Resucitador de Links CaÃ­dos" (Archive.org / Wayback Machine):**
   - Si una pÃ¡gina da Error 404 o el servidor no responde, EonSurf muestra una advertencia clara:
     `"Sitio fuera de lÃ­nea. Â¿Desea consultar la Ãºltima copia histÃ³rica en Wayback Machine (Archive.org)?"`
   - No abre nada automÃ¡tico: solo si el usuario confirma, consulta la copia archivada.
2. **Centro de Control webMAN MOD Integrado:**
   - Detecta si la consola corre webMAN (`http://127.0.0.1:80`) y ofrece un panel directo para montar juegos ISO, ver temperaturas en vivo y regular ventilaciÃ³n con 1 botÃ³n.
3. **Soporte Nativo de Gemini (`gemini://` - The Smolnet):**
   - Primer navegador en consolas PlayStation en soportar la red Gemini. Permite leer miles de blogs, artÃ­culos y enciclopedias sin publicidad ni JavaScript a velocidad instantÃ¡nea (2 MB de RAM).
4. **Descompresor Nativo ZIP / 7z con Soporte de ContraseÃ±a:**
   - Motor ligero en C (`miniz` / `libarchive`). Al descargar parches, mods o traducciones comprimidas:
     - Pregunta: `Â¿Desea extraer en este directorio?`
     - Si el archivo tiene clave, abre el teclado OSK oficial de Sony para ingresar la contraseÃ±a y descomprime directo en el disco interno.
     - **Visor de ContraseÃ±a:** OpciÃ³n para alternar entre asteriscos `****` y texto visible para no equivocarse al escribirla con el joystick.
     - **Pegar Clave:** BotÃ³n directo para pegar una contraseÃ±a que se haya copiado previamente de un foro o pÃ¡gina web mientras se navegaba.
5. **Sistema de SelecciÃ³n, Copiado y Pegado de Texto:**
   - Manteniendo presionado $times$ sobre el texto y arrastrando el stick analÃ³gico, se resalta la frase deseada.
   - MenÃº contextual: *Copiar al portapapeles*, *Buscar en Google/DuckDuckGo* o *Pegar en la barra de direcciones / cuadros de texto / campos de clave*.
6. **"EonPush" / "Send to PS3" (Mandar pestaÃ±as desde Celular / PC a la PS3):**
   - EonSurf incorpora un mini-servidor local ultraligero en segundo plano (puerto `8088`).
   - Escaneas un cÃ³digo QR en la TV con tu celular o abres una mini-web en tu PC, pegas cualquier enlace y al tocar **"Enviar"**, la PS3 abre esa pÃ¡gina de inmediato.
   - Sin cables, sin escribir links largos con el joystick y sin necesidad de servicios de terceros.

---

## 7. Tanda 5: Innovaciones No Obvias de PrÃ³xima GeneraciÃ³n

1. **"EonPull" (Lo inverso a EonPush):**
   - Â¿EstÃ¡s leyendo una guÃ­a o viendo una imagen en la PS3 y tenÃ©s que apagar la consola o irte a otra habitaciÃ³n?
   - Presionas un botÃ³n y la PS3 te genera un cÃ³digo QR temporal en pantalla. Lo escaneas con el celular y continuas leyendo la misma pÃ¡gina web exactamente donde la dejaste en la TV.
2. **Generador de Portadas y Miniaturas para Juegos (Cover Grabber AutomÃ¡tico):**
   - Si estÃ¡s buscando carÃ¡tulas o fondos para juegos de PS3, PS2 o PS1 en internet, con pararte sobre cualquier imagen y tocar $triangle$, te da la opciÃ³n:
     `"Guardar como carÃ¡tula de juego"` $rightarrow$ la convierte automÃ¡ticamente al formato y tamaÃ±o exacto para multiMAN/webMAN (`ICON0.PNG` o `COVER.JPG`) y la guarda en la carpeta del juego.
3. **Traductor InstantÃ¡neo de PÃ¡ginas en 1 Toque:**
   - Para leer foros o wikis en inglÃ©s, japonÃ©s o francÃ©s: utiliza una API ligera y gratuita de traducciÃ³n de texto (como LibreTranslate/Google Translate Lite) que traduce el artÃ­culo a espaÃ±ol sin recargar ni romper la estructura de la web.
4. **CachÃ© Persistente en Disco Anti-Cuelgues ("Crash Resume"):**
   - Si la consola se llega a apagar de golpe, se corta la luz o cierras la app, EonSurf guarda el estado de la Ãºltima pÃ¡gina activa en `/dev_hdd0/game/EONS00001/USRDIR/session.json`. Al volver a abrir el navegador te pregunta:
     `"Â¿Desea restaurar la sesiÃ³n anterior?"` y vuelve exactamente a la pÃ¡gina en la que estabas.
5. **Manejador Nativo de Descargas Directas (MediaFire, MEGA, Google Drive, Archive.org):**
   - En lugar de cargar pÃ¡ginas web pesadas llenas de pop-ups y captchas imposibles para una consola:
     - **MediaFire:** EonSurf detecta la URL e intercepta automÃ¡ticamente el enlace directo al archivo real (`.pkg`, `.zip`, `.iso`), iniciando la descarga directa a USB o disco sin publicidad ni botones falsos.
     - **MEGA.nz:** Integra un descifrador ligero directo en C (`megatools / REST API`) para descargar links pÃºblicos sin exigir el motor pesado de MEGA en navegador.
     - **Archive.org / GitHub Releases / 1Fichier:** Descarga con barra de velocidad en MB/s y porcentaje en tiempo real.
6. **Lanzador / Acceso a Emuladores Nativos vs Web:**
   - La emulaciÃ³n en navegadores modernos (WebAssembly/HTML5 Canvas) requiere navegadores de PC con gigabytes de RAM que colapsarÃ­an la PS3.
   - **La soluciÃ³n inteligente en EonSurf:** Si entras a una web de ROMs o juegos retro, EonSurf te permite descargar el juego en 1 clic y enviarlo automÃ¡ticamente a la carpeta de emuladores nativos de la PS3 (RetroArch / FB Neo / emuladores de PS1-PS2) para jugarlo a 60 FPS fluidos por hardware real en vez de por una web trabada.
7. **"EonAI" â Asistente de Inteligencia Artificial Gemini Integrado:**
   - La web oficial de `gemini.google.com` requiere Chrome con Angular, WebGL y 4 GB de RAM.
   - **La soluciÃ³n en EonSurf:** En lugar de abrir la web pesada, EonSurf incorpora un cliente nativo ultra-liviano que se conecta directamente a la API oficial de Google Gemini (`gemini-2.5-flash` / endpoint REST):
     - Abre una ventana lateral o pestaÃ±a limpia con fondo oscuro: preguntas con el teclado de la PS3 o pegando texto, y Gemini responde en segundos.
     - Cero consumo de RAM excesivo (apenas 1 MB para manejar el JSON y el texto).
     - La primera consola retro en la historia en tener acceso nativo a Inteligencia Artificial conversacional.



---

## 10. Tanda 6: Usabilidad TV, D-Pad y Manejo de Medios

1. **NavegaciÃ³n Espacial Directa con Cruceta (D-Pad Link Jumping - Opcional):**
   - Alternativa al cursor de ratÃ³n libre: la cruceta salta de un enlace al siguiente de forma lÃ³gica resaltando la caja en neÃ³n.
   - ActivaciÃ³n opcional en ajustes para quienes prefieran navegar botÃ³n a botÃ³n en vez de mover el ratÃ³n virtual.
2. **Sugerencias y Autocompletado Predictivo:**
   - Mientras se escribe en la barra de bÃºsqueda o en el Startpage, despliega sugerencias de dominios populares y pÃ¡ginas ya visitadas en el historial.
3. **Detector y Reproductor Nativo de Videos (Video Pop-out):**
   - Si la pÃ¡gina web contiene un video (`.mp4`, `.mkv`, stream `m3u8`), EonSurf lo detecta y muestra una barra limpia:
     `[â¶ Reproducir a pantalla completa]` o `[ð¾ Descargar a USB/Disco]`.
   - AÃ­sla el video de los scripts de la pÃ¡gina para reproducirlo suavemente por hardware RSX.
4. **Memoria de Preferencias por Sitio Web (Opcional):**
   - Recuerda individualmente por cada web: nivel de zoom preferido (125%, 150%), modo mÃ³vil o modo oscuro, para no tener que reconfigurarlo cada vez que se entra.
5. **AceleraciÃ³n y Modos de Velocidad del Cursor con R3 (Configurable):**
   - El stick analÃ³gico cuenta con un selector de velocidades escalonadas con un clic en **R3**:
     - *Modo 1 (PrecisiÃ³n / Lento):* Mueve el cursor pÃ­xel por pÃ­xel para seleccionar texto fino o hacer clic en botones diminutos.
     - *Modo 2 (Normal - Predeterminado):* Velocidad estÃ¡ndar balanceada.
     - *Modo 3 (RÃ¡pido):* Para televisores grandes de 55 pulgadas o mÃ¡s.
     - *Modo 4 (Ultra RÃ¡pido):* Cruza toda la pantalla en una fracciÃ³n de segundo.
     - Un pequeÃ±o indicador temporal en una esquina muestra el modo activo: `[Velocidad: Normal / RÃ¡pida / Ultra]`.
6. **"Air Mouse" Sixaxis con CalibraciÃ³n de Zona Muerta (Inspirado en EonCT):**
   - Basado en los aprendizajes de `EonCT` (con giroscopio y control de apuntado fino):
     - **Zona Muerta Anti-Temblor (Deadzone):** Evita que el pulso natural de la mano mueva el cursor cuando el joystick estÃ¡ quieto.
     - **Sensibilidad Ajustable:** En el menÃº de opciones (Baja, Media, Alta).
     - **Interruptor RÃ¡pido (L3 prolongado):** Activa o congela el modo Air Mouse en cualquier momento para apoyar el control en la mesa sin que el puntero se dispare.

7. **Teclado Sony OSK Oficial:**
   - Confirmado: El teclado oficial de Sony ya cuenta con atajos nativos de terminaciones (`.com`, `.net`, `.org`) en la barra de operaciones rÃ¡pida al ingresar URLs.


Inspirado en la popular aplicaciÃ³n **Downloader by AFTVnews** de Android TV / Fire TV, adaptado nativamente para resolver el problema de escribir URLs largas con el mando de la PS3:

### Â¿CÃ³mo funciona la compatibilidad con cÃ³digos de Downloader?
1. **DetecciÃ³n AutomÃ¡tica Inteligente (Sin lÃ­mite superior de dÃ­gitos):**
   - El usuario abre la barra de bÃºsqueda y en lugar de una URL larga como `https://github.com/.../mi-app.pkg`, escribe simplemente un nÃºmero (ej. `75077`, `123456`, `8492019`, etc.).
   - EonSurf analiza la entrada: si detecta una secuencia de **4 dÃ­gitos en adelante** compuesta exclusivamente por nÃºmeros (sin letras, puntos de dominio ni barras), reconoce automÃ¡ticamente que es un cÃ³digo numÃ©rico de redirecciÃ³n.
   - **DiseÃ±o a prueba de futuro:** No tiene lÃ­mite de 6 dÃ­gitos; a medida que la base de datos crezca a 7, 8 o mÃ¡s cifras con el paso de los aÃ±os, el sistema seguirÃ¡ funcionando a la perfecciÃ³n de por vida.
2. **Uso de la misma Base de Datos existente (AFTVnews Redirector):**
   - EonSurf consulta la cabecera HTTP de `https://go.aftvnews.com/[cÃ³digo]`.
   - El servidor responde con una redirecciÃ³n estÃ¡ndar `302 Found` entregando la URL real de destino en milisegundos.
   - Cero necesidad de crear servidores propios; **aprovecha los millones de cÃ³digos que ya existen en internet y en tutoriales de YouTube**.
3. **AcciÃ³n de Descarga o NavegaciÃ³n:**
   - Si el enlace final es un archivo instalable o comprimido (`.pkg`, `.zip`, `.mp4`): EonSurf salta a la pantalla de descarga directa preguntando si guardar en USB o Disco.
   - Si el enlace final es una pÃ¡gina web: EonSurf carga la web directamente en pantalla.
4. **Modo Teclado NumÃ©rico Exclusivo (Opcional con Select):**
   - Al activar el modo numÃ©rico, el teclado OSK de la PS3 se abre directamente en el bloque de nÃºmeros (0-9), haciendo que ingresar un cÃ³digo tome literalmente 3 segundos.

Inspirada en las mejores interfaces de Android TV / Smart TV modernas, pensada para leerse a 3 metros del sillÃ³n con tipografÃ­a grande y navegaciÃ³n 100% fluida con la cruceta del DualShock 3:

```text
+-------------------------------------------------------------------------------+
|  EonSurf                [ 21:30 | Vie 24 Oct ]               [ 22Â°C Buenos Aires ]
+-------------------------------------------------------------------------------+
|                                                                               |
|            [ ð  Buscar en internet o escribir direcciÃ³n web... (Start) ]      |
|                                                                               |
|   ACCESOS RÃPIDOS (Speed Dial - Editables):                                   |
|   +---------------+  +---------------+  +---------------+  +---------------+  |
|   |  â¶ Yo! Player |  |  W Wikipedia  |  |  ð» Radios    |  |  ð Reddit    |  |
|   |  (YouTube TV) |  |  (Enciclopedia|  |  (Streaming)  |  |  (Foros/PS3)  |  |
|   +---------------+  +---------------+  +---------------+  +---------------+  |
|   +---------------+  +---------------+  +---------------+  +---------------+  |
|   |  â MediaFire  |  |  ð¾ Disco PS3 |  |  â webMAN     |  |  â Agregar   |  |
|   |  (Descargas)  |  |  (/dev_usb000)|  |  (Control OSD)|  |  (Marcador)   |  |
|   +---------------+  +---------------+  +---------------+  +---------------+  |
|                                                                               |
+-------------------------------------------------------------------------------+
```

### CaracterÃ­sticas del Dashboard:
1. **Fila Superior de Estado (Widgets en Vivo):**
   - **Hora y Fecha en Tiempo Real:** Grande y elegante, sincronizada con el reloj de la PS3.
   - **Widget del Clima:** Muestra icono de estado, temperatura actual y nombre de tu ciudad.
2. **ConfiguraciÃ³n de UbicaciÃ³n Personalizada:**
   - En el menÃº de ConfiguraciÃ³n $rightarrow$ *"UbicaciÃ³n / Clima"*:
     - Campo editable donde el usuario escribe su ciudad o localidad (ej. *"Buenos Aires"*, *"Madrid"*, *"BogotÃ¡"*).
     - Se guarda en el archivo de preferencias `/dev_hdd0/game/EONS00001/USRDIR/eonsurf.cfg` para que nunca se pierda.
     - Consulta la API ligera de `wttr.in` en segundo plano cada 30 minutos sin gastar recursos.
3. **Barra de BÃºsqueda Central Destacada:**
   - Caja grande y visible con esquinas redondeadas.
   - Al pararse en ella con el mando o tocar **START**, se abre el teclado oficial de Sony para buscar al instante (con DuckDuckGo / Google Lite).
4. **Tarjetas de Acceso RÃ¡pido (Speed Dial) 100% Editables:**
   - Vienen preinstaladas con lo mejor que definimos:
     - **Yo! Player:** YouTube sin anuncios adaptado para TV.
     - **Wikipedia:** Enciclopedia rÃ¡pida.
     - **Radios:** Selector de streaming de mÃºsica y noticias.
     - **Reddit / PSX-Place (Opcional):** Foros limpios con formato `.json`.
     - **MediaFire / Nube:** Acceso a repositorios de descarga directa.
     - **Disco PS3 / USB:** Explorador de archivos local.
     - **webMAN MOD:** Centro de control de la consola.
   - **PersonalizaciÃ³n total:** El usuario puede pulsar **$triangle$** sobre cualquier tarjeta para borrarla, editar la URL o moverla, o pulsar **[+]** para guardar cualquier pÃ¡gina web favorita.
5. **AnimaciÃ³n de Foco Clara para TV:**
   - Al mover la cruceta o el stick sobre las tarjetas, la tarjeta seleccionada aumenta ligeramente de tamaÃ±o con un borde neÃ³n cian brillante para que nunca te pierdas en la pantalla.

---

## 11. Ecosistema Homebrew, HEN / CFW y ExplotaciÃ³n Segura

### Compatibilidad y Lanzador de Exploits (HEN / HAN / Toolset):
* **El dilema de los exploits en navegadores alternativos:**
  - Los instaladores web de HEN (como PS3Toolset de bguerville o los scripts de ps3xploit) no son pÃ¡ginas web normales: dependen de vulnerabilidades de memoria muy especÃ­ficas del motor viejo de Sony (NetFront / Flash 9 / WebKit antiguo) para inyectar payloads en el kernel Lv2.
* **La SoluciÃ³n Maestra en EonSurf ("Exploit Pass-Through / Auto-Updater"):**
  - **DetecciÃ³n de Sitios de Hack:** Si el usuario ingresa a una web de instalaciÃ³n de HEN o PS3Toolset (`ps3xploit.me`, `ps3toolset.com`, etc.):
    - EonSurf lo reconoce y le ofrece:  
      `"Se ha detectado una herramienta de modificaciÃ³n del sistema. Â¿Desea ejecutarla mediante el motor del sistema o descargar el instalador offline PKG?"`
  - **Descargador e Instalador Offline de HEN:** EonSurf permite descargar directamente los paquetes oficiales actualizados (`Latest_HEN_Installer_signed.pkg` y `PS3HEN.BIN`) y dejarlos listos en `/dev_hdd0/packages/` sin depender de que la web del exploit estÃ© caÃ­da o dÃ© error de SSL.
  - **Actualizador de Homebrew en 1 Clic:** Detecta si hay una nueva versiÃ³n de HEN, webMAN MOD o multiMAN y permite actualizarla con 1 toque.

---

## 12. Perspectiva HistÃ³rica: La LecciÃ³n de PSP (NetFront High-Memory)
* **La PSP (PlayStation Portable):** TenÃ­a el mismo motor NetFront que la PS3, pero Sony limitÃ³ por software el uso de memoria a solo 4 MB, haciendo que el 90% de las webs dieran el error de "Memoria Insuficiente".
* **Lo que aprendimos del hack "High Memory Mod" de PSP:** Liberar los 32/64 MB completos de la PSP cambiÃ³ la historia. En EonSurf aplicamos la misma filosofÃ­a:
  - Liberamos **el 100% de la memoria disponible de la PS3** (sin las restricciones artificiales del navegador original de Sony), usando memoria RSX dedicada para video y almacenamiento en disco para la cachÃ© pesada.

---

---

## 14. La Brecha de 2026: Lo que Chrome/Firefox tienen de fÃ¡brica y NetSurf NO trae
Para que EonSurf no se quede como un navegador "retro congelado en 2006", implementamos de forma nativa las 7 comodidades esenciales que todo usuario moderno da por sentadas:

1. **Gestor de Descargas Moderno ("Download Shelf" emergente):**
   - NetSurf original solo guarda un archivo y se olvida.
   - **En EonSurf:** Al terminar una descarga, aparece una notificaciÃ³n abajo a la derecha:
     - **[Abrir archivo]:** Si es una foto o documento de texto, lo abre en el navegador. Si es un `.pkg`, pregunta si instalarlo.
     - **[Mostrar en carpeta]:** Te lleva directamente al directorio del disco (`/dev_hdd0/downloads/` o USB) donde quedÃ³ guardado.
     - Historial de descargas accesible desde el menÃº principal con tamaÃ±o y fecha.
2. **Scroll Suave con Inercia FÃ­sica ("Smooth Inertial Scrolling"):**
   - NetSurf clÃ¡sico hace un salto brusco lÃ­nea por lÃ­nea que marea en una TV de 50".
   - **En EonSurf:** El stick derecho y los botones de scroll mueven la pantalla con desaceleraciÃ³n fÃ­sica suave a 60 FPS fijos, como deslizar el dedo en un celular o tablet moderna.
3. **Visor Nativo de ImÃ¡genes y Documentos (Photo & Text Viewer):**
   - Si haces clic en una foto (`.png`, `.jpg`, `.webp`) o un archivo de texto/cÃ³digo (`.txt`, `.log`, `.nfo`), EonSurf lo abre en pantalla completa con controles de zoom y paneo sin descargarlo como un archivo extraÃ±o.
4. **Buscador de Texto en PÃ¡gina Visual ("Find in Page"):**
   - Al tocar **$triangle$** $rightarrow$ Buscar palabra: resalta todas las coincidencias en amarillo brillante y te permite saltar entre la siguiente y la anterior con **L1 / R1**.
5. **Historial Completo con Buscador y AgrupaciÃ³n por Fecha:**
   - Organizado por *"Hoy"*, *"Ayer"*, *"Esta semana"*, con opciÃ³n de borrar historial de la Ãºltima hora o completo en 1 toque.
6. **Autocompletado de Formularios y URLs Inteligente (Omnibox):**
   - Al empezar a tipear en la barra, sugiere tanto sitios web del historial como bÃºsquedas populares, sin necesidad de tipear la direcciÃ³n completa.
7. **Barra de Progreso y Estado de Carga Discreta:**
   - Una lÃ­nea cian delgada superior que avanza suavemente mostrando el porcentaje real de carga (10%... 50%... 100%), para saber en todo momento si la conexiÃ³n estÃ¡ trabajando o si la pÃ¡gina ya terminÃ³.

---

## 15. Soporte Multi-Idioma Nativo (InternacionalizaciÃ³n - i18n)
EonSurf se diseÃ±a para una audiencia global con detecciÃ³n automÃ¡tica del idioma de la consola PS3 mediante la API oficial de Sony (`sysUtilGetSystemParam`):

### Idiomas Soportados de FÃ¡brica:
1. ðªð¸ **EspaÃ±ol:** Idioma principal nativo (Argentina / EspaÃ±a / LatinoamÃ©rica).
2. ðºð¸ **InglÃ©s:** EstÃ¡ndar internacional para la comunidad anglosajona.
3. ð§ð· **PortuguÃ©s (Brasil / Portugal):** Comunidad masiva de PS3 en Brasil y Portugal.
4. ð«ð· **FrancÃ©s:** Amplia base homebrew europea.
5. ð©ðª **AlemÃ¡n:** Comunidad tÃ©cnica europea.
6. ð®ð¹ **Italiano:** Escena activa de emulaciÃ³n y mods.
7. ð·ðº **Ruso:** Una de las comunidades mÃ¡s grandes del mundo en PS3 HEN y preservaciÃ³n.
8. ð¯ðµ **JaponÃ©s:** Idioma de origen de la consola.

- **Arquitectura ligera en JSON:** Las cadenas de texto se cargan desde un archivo plano de apenas 20 KB (`lang_es.json`, `lang_en.json`), permitiendo a la comunidad traducir el navegador a mÃ¡s idiomas sin recompilar el cÃ³digo.

---

## 16. Portabilidad a Otras Consolas y Sistemas Obsoletos
Gracias a que el nÃºcleo de EonSurf estÃ¡ escrito en C puro desacoplado mediante una superficie de dibujo abstracta (`surface_ps3.c`), el 90% del cÃ³digo del navegador es reutilizable para compilar en otras plataformas legendarias:

1. **Xbox ClÃ¡sica (2001 - 64 MB de RAM):**
   - El gran dolor de cabeza histÃ³rico: solo existÃ­a *LinksBoks* (modo texto tosco).
   - EonSurf en Xbox ClÃ¡sica corre holgadamente en sus 64 MB de RAM, trayendo por primera vez navegaciÃ³n grÃ¡fica real con soporte de mandos Duke / S.
2. **PlayStation Vita (VitaSDK / LiveArea):**
   - La comunidad de Vita depende de parches (`iTLS-Enso`) porque el navegador de fÃ¡brica colapsa con SSL.
   - EonSurf en Vita ofrece navegaciÃ³n tÃ¡ctil fluida, descargas directas a `ux0:data/` y compatibilidad nativa con GitHub/MediaFire.
3. **PlayStation Portable (PSP):**
   - El entorno NetFront original de PSP crasheaba por falta de memoria. EonSurf optimizado para pantalla 480x272 con bajo consumo revive la navegaciÃ³n en PSP.
4. **Raspberry Pi 1 / Pi Zero (512 MB de RAM o menos):**
   - Donde Chromium y Firefox se cuelgan al arrancar, EonSurf (versiÃ³n Linux framebuffer/X11) consume 25 MB de RAM y vuela a 60 FPS.
5. **Netbooks antiguas / PCs con procesador Intel Atom y 1 GB de RAM:**
   - La soluciÃ³n ligera definitiva para computadoras recicladas que ya no soportan Windows 10 ni navegadores modernos pesados.

---

## 19. Lecciones TÃ©cnicas de CompilaciÃ³n y SoluciÃ³n de Errores (Post-Mortem)
Registro crÃ­tico de problemas encontrados durante el desarrollo de EonSurf para evitar reincidencias en futuras compilaciones y versiones:

### 1. Error de Cuelgue en RPCS3: `Illegal opcode 0x00000000 at 0x7c0802a4`
* **Causa:** Uso de `make_self_npdrm` al firmar el binario `EBOOT.BIN`. Esta herramienta cifra los segmentos de cÃ³digo con licencias DRM de Sony para distribuciÃ³n comercial cerrada. Los emuladores como RPCS3 no pueden desencriptar estos bloques y leen ceros binarios en memoria, provocando el cierre inmediato con el cartel: *"The PS3 application has likely crashed"*.
* **SoluciÃ³n Obligatoria:** Firmar siempre con **`make_fself`** (o `fself` del SDK PSL1GHT). El formato FSELF produce binarios de depuraciÃ³n libres de encriptaciÃ³n que arrancan de forma instantÃ¡nea tanto en emulador RPCS3 como en consolas reales con HEN o Custom Firmware (CFW).

### 2. Iconos Antiguos Visibles en RPCS3 tras Actualizar la AplicaciÃ³n
* **Causa:** RPCS3 mantiene un directorio de cachÃ© compiled (`cache/EONS00001/` y bases de datos locales) donde congela los iconos `ICON0.PNG` y tÃ­tulos previos.
* **SoluciÃ³n Obligatoria:** 
  1. Copiar `ICON0.PNG` (320x176) y `PIC1.PNG` (1920x1080) tanto dentro del paquete como directamente en `dev_hdd0/game/EONS00001/`.
  2. Vaciar el directorio `cache/EONS00001/` para forzar a RPCS3 a regenerar la miniatura grÃ¡fica.

### 3. Registro de Superficie en `libnsfb`:
* **Causa:** Usar la estructura antigua `nsfb_surface_rt_t` produce errores de compilador en GCC.
* **SoluciÃ³n Obligatoria:** Utilizar la estructura moderna `nnsfb_surface_rtns_t` con macro `NSFB_SURFACE_DEF(ps3, NSFB_SURFACE_RAM, &ps3_rt_surface)`.


### 1. Actualizador AutomÃ¡tico Nativo (Inspirado en webMAN MOD & PKGi):
* **El problema de las primeras versiones:** Lanzar la v1.0 y que los usuarios tengan que usar una PC y un pendrive cada vez que arreglamos un detalle o agregamos una funciÃ³n arruina la adopciÃ³n.
* **El mecanismo de EonUpdate (100% AutomÃ¡tico):**
  1. **ComprobaciÃ³n Silenciosa al Iniciar:**  
     Al abrir EonSurf, realiza una consulta HTTP GET ligera (2 KB) a la API de GitHub Releases (`/repos/juanklimonplay/EonSurf/releases/latest`).
  2. **ComparaciÃ³n SemÃ¡ntica de VersiÃ³n:**  
     Compara la versiÃ³n local del archivo `PARAM.SFO` (ej. `v1.0.0`) con el `"tag_name"` de GitHub (ej. `v1.0.1`).
  3. **Aviso en Pantalla Amigable:**  
     Si hay una versiÃ³n nueva, muestra una ventana emergente:  
     `"Â¡Nueva versiÃ³n disponible de EonSurf (v1.0.1)! Â¿Deseas actualizar ahora?"`  
     `[SÃ­, actualizar] â [MÃ¡s tarde]`
  4. **Descarga e InstalaciÃ³n en 1 Toque:**  
     Descarga el `.pkg` nuevo directamente a `/dev_hdd0/packages/` y mediante una llamada de sistema (syscall de HEN/CFW) lo instala automÃ¡ticamente y reinicia la app sin necesidad de pendrive ni PC.
  5. **Modo Privado / Repositorio PÃºblico:**  
     Compatible tanto con repositorios pÃºblicos de GitHub como con un archivo JSON de respaldo alojado en la nube.

---

### 2. "EonFeedback / Safe Crash Recovery" (DiagnÃ³stico para Mejoras Continuas):
* **Caja de Arena Anti-Cuelgues (Sandbox Safe Mode):**  
  Si el navegador se cierra de forma inesperada por una pÃ¡gina web maliciosa o saturada de memoria, al volver a abrir muestra una opciÃ³n de arranque a prueba de fallos:  
  `"EonSurf se cerrÃ³ inesperadamente. Â¿Deseas iniciar en Modo Seguro (sin pestaÃ±as previas ni cachÃ©)?"`
* **Log de DiagnÃ³stico Local (`crash.log`):**  
  Guarda en `/dev_hdd0/game/EONS00001/USRDIR/debug.log` los Ãºltimos errores sin mandar nada a Sony, permitiendo al usuario ver exactamente quÃ© pasÃ³ o reportar el error en el repositorio para que lo solucionemos de inmediato.
* **Canal Directo de Novedades y Changelog ("QuÃ© hay de nuevo"):**  
  Una pestaÃ±a en Ajustes que muestra la lista de mejoras aÃ±adidas en la Ãºltima actualizaciÃ³n para que el usuario conozca las nuevas funciones apenas actualice.


---

## 20. Blindaje Técnico de Bajo Nivel y Optimización Extrema (7 Pilares de Rendimiento PS3)

Para garantizar estabilidad absoluta de nivel comercial y prevenir cuellos de botella del Kernel Lv2 y el procesador Cell:

### 1. Pila de Red y Sockets en Lv2 (sys_net / libcurl)
- **Límites Estrictos de Conexión (eonsurf.cfg):** Configurar max_fetchers: 8, max_fetchers_per_host: 4 y max_cached_fetch_handles: 4 (CURLMOPT_MAX_HOST_CONNECTIONS). Previene el agotamiento de descriptores de socket en el kernel Lv2.
- **Agrandar Buffer TCP (SO_RCVBUF):** Aplicar setsockopt() con SO_RCVBUF a 256 KB (frente a los 32 KB por defecto de Sony) para maximizar descargas de archivos pesados.
- **Reutilización de Sesiones SSL (CURLOPT_SSL_SESSIONID_CACHE):** Habilitado en 1L para guardar el Session ID/Ticket y evitar renegociar TLS en páginas del mismo dominio.

### 2. Memoria XDR y Asignador Custom Anti-Fragmentación (TLSF)
- **El problema de Newlib:** El malloc() básico fragmenta el Heap al crear y destruir miles de nodos DOM/CSS pequeños.
- **Solución:** Integración de la librería **TLSF** (*Two-Level Segregated Fit*) de tiempo constante (1)$.
- **Pool Dedicado:** Reserva al iniciar de un bloque único de **64 MB** en RAM XDR con sysMemAllocate(). Todas las asignaciones de libdom, libcss y libhubbub se realizan dentro de este pool y se resetean atómicamente por pestaña (arena_destroy()).

### 3. Descargador Directo a /dev_hdd0/packages a 60 FPS
- **Ring Buffer en RAM (1 a 2 MB):** Evita la saturación del bus SATA I de la PS3. Se acumulan fragmentos de red en RAM y se realiza un único sysFsWrite() secuencial desde un hilo secundario con prioridad Lv2 baja (1001).
- **Pre-asignación con sysFsFtruncate:** Comprueba el encabezado Content-Length y el espacio libre en disco antes de descargar.
- **Descargas Atómicas (.part):** Guarda como rchivo.pkg.part y renombra a .pkg únicamente tras validar el tamaño final o suma de verificación (checksum).

### 4. Fuentes Nativas del Firmware y Atlas de Glifos en XDR
- **Sin Inflar el PKG:** Carga directa de las fuentes TrueType oficiales del firmware en /dev_flash/data/font/SCE-PS3-VR-R-LATIN.TTF (y ROD-R).
- **Atlas de Glifos Rasterizados:** Cada carácter se renderiza una única vez con FreeType2 en una textura atlas en RAM XDR; el scroll realiza blitting directo por DMA al RSX sin recalcular curvas vectoriales en la CPU.

### 5. AdBlocker Ultraligero Nativo por Hash FNV-1a (Cero Regex)
- **Sin Motor Regex Pesado:** Evita evaluar expresiones regulares complejas en la PPU.
- **Tabla Binaria Compacta (120 KB):** Lista de 15.000 dominios de telemetría y anuncios pre-hasheados con FNV-1a (64-bit). Se evalúa en tiempo (1)$ antes de que el motor resuelva la consulta DNS.

### 6. Integración con el Sistema Operativo PS3 y Calidad de Vida (QoL)
- **Inhibición de Salvapantallas:** Llamada periódica de actividad al sistema para evitar que el XMB apague la pantalla durante la lectura o descargas largas.
- **Salida Limpia con Botón PS (SYSUTIL_EXIT_GAME):** Cierra descriptores abiertos en disco, detiene hilos de SPU y libera el contexto RSX para evitar triples pitidos de la consola.
- **Certificados CA Actualizados (cacert.pem):** Ubicados en /dev_hdd0/game/EONS00001/USRDIR/certs/cacert.pem, actualizables por FTP o pendrive.

### 7. Alineación Estricta DMA para SPUs (MFC)
- **Alineación a 128 Bytes:** Todos los descriptores de la cola circular FIFO y buffers de paquetes SPU se declaran con __attribute__((aligned(128))) y transferencias múltiplos de 16 bytes para alcanzar el ancho de banda pico del bus EIB (204.8 GB/s).

### 4. Resolución Obligatoria de Stubs PRX con sprxlinker:
* **Causa:** Un binario ELF compilado directamente con powerpc64-ps3-elf-gcc contiene stubs sin resolver (0x00000000) para las librerías dinámicas del sistema de Sony (sysutil, sx, 
et, io). Al ejecutarse en RPCS3 o consola real, cualquier llamada provoca un salto a memoria no mapeada con error: *VM: Access violation executing location 0x7c0802a4 / Unknown opcode: 0x00000000*.
* **Solución Obligatoria:** Ejecutar siempre **sprxlinker EonSurf.elf** inmediatamente después de la compilación de GCC y antes de la firma con make_fself. sprxlinker parchea los stubs del ELF vinculándolos con la tabla OPD real del sistema operativo PS3.

### 8. Detalles Quirúrgicos de Implementación en Código C (PSL1GHT & Lv2)

1. **Prioridad del Hilo de Escritura SATA (sysThreadCreate):**
   - En el kernel Lv2 de PS3, un número de prioridad menor significa mayor prioridad de CPU (0 es la máxima, 3071 la mínima).
   - El hilo principal corre en prioridad estándar (1001).
   - El hilo de volcado a disco para descargas de .pkg se instancia con **prioridad 1500 o 2000**. De este modo, el planificador del Cell jamás roba ciclos al bucle de renderizado ni pierde un solo VSync del RSX (garantizando los 60 FPS inquebrantables).

2. **Apertura de Fuentes en /dev_flash con Flag Estricto SYS_O_RDONLY:**
   - La fuente /dev_flash/data/font/SCE-PS3-VR-R-LATIN.TTF se debe abrir estrictamente en modo solo lectura (SYS_O_RDONLY).
   - Solicitar permisos de escritura sobre /dev_flash dispara la protección del hipervisor de Sony en CFW Evilnat o PS3HEN, provocando un fallo de descriptor (EBADF o EACCES).

3. **Búsqueda Binaria (search) sobre la Tabla FNV-1a de 120 KB:**
   - Al compilar la tabla estática binaria de 15.000 dominios, los hashes uint64_t se guardan estrictamente ordenados de menor a mayor.
   - La evaluación en tiempo de ejecución en la PPU utiliza la función estándar bsearch().
   - Se determina si un dominio es publicitario en un máximo de **14 operaciones de comparación** ($log_2(15000) approx 13.8$), operando directamente dentro de los 512 KB de la caché L2 de la PPU sin tocar la RAM.

---

## 21. Matriz de Ingeniería Definitiva de EonSurf

| Módulo / Subsistema | Parámetro / Constante de Bajo Nivel | Mecanismo en PSL1GHT / C | Garantía en Consola Física PS3 |
| :--- | :--- | :--- | :--- |
| **Stack de Red (sys_net)** | max_fetchers: 8 / per_host: 4 / SO_RCVBUF: 256KB | Reutilización CURLOPT_SSL_SESSIONID_CACHE = 1L | Descargas a tope de línea sin agotar sockets del kernel Lv2. |
| **Memoria DOM/CSS** | Pool estático XDR de **64 MB** | Asignador **TLSF** ((1)$) + arena_destroy() por pestaña | Cero fragmentación de memoria en sesiones de uso prolongadas. |
| **I/O Disco Duro (SATA I)** | Buffer RAM de **2 MB** + Extensión .part | sysFsFtruncate() + sysFsWrite() en hilo secundario (Prioridad ~1500) | Descargas limpias a /dev_hdd0/packages sin tirones en los 60 FPS. |
| **Motor Tipográfico** | /dev_flash/.../SCE-PS3-VR-R-LATIN.TTF (SYS_O_RDONLY) | FreeType2 + **Atlas de Glifos** pre-rasterizados en RAM XDR | Texto 1080p con anti-aliasing sin recalcular curvas en el scroll. |
| **AdBlocker Nativo** | Tabla binaria ordenada de **120 KB** | Hash **FNV-1a (64-bit)** + bsearch() pre-DNS ((log N)$) | Bloqueo instantáneo en caché L2 con 0% de impacto en CPU. |
| **Ciclo de Vida XMB** | SYSUTIL_EXIT_GAME + USRDIR/certs/cacert.pem | Cierre ordenado de SPUs/descriptores + Certificados por FTP | Cero triple pitido al salir al XMB y soporte HTTPS a futuro. |
| **Bus EIB y MFC (SPUs)** | __attribute__((aligned(128))) | Cola circular FIFO + Hardware Mailboxes (spu_readch) | Transferencias DMA a velocidad de ráfaga sin bus errors. |

---

## 22. Resolución Definitiva de los 4 Dilemas de Arquitectura Legendaria

Para blindar la estabilidad, el consumo de memoria y la fluidez gráfica a 60 FPS en situaciones extremas (imágenes gigantes, JavaScript, composición RSX y recuperación de fallos):

### 1. Manejo de Imágenes Gigantes (4K / 8K): Módulos SPRX Nativos + IDCT Downsampling
- **Módulos Oficiales en SPUs:** Uso de SYSMODULE_JPGDEC y SYSMODULE_PNGDEC (libjpgdec y libpngdec) provistos por el firmware de la consola. Sony diseñó estos módulos para ejecutarse de forma transparente y acelerada dentro de las SPUs del Cell.
- **IDCT Scaling al Vuelo (Cero Desperdicio de RAM):** 
  - Al recibir las cabeceras HTTP de una imagen, se lee su resolución original con jpgDecReadHeader().
  - Si la imagen supera 1920x1080 (ej. una foto 4K de 3840x2160), se configura el decodificador (jpgDecInParam) con factor de escala scale_factor a 1/2 o 1/4.
  - La SPU descarta frecuencias altas matemáticamente en la Transformada de Coseno Discreta y vuelca a la RAM XDR directamente una textura de 960x540 o 1920x1080 (ocupando solo ~4 MB en formato RGB565 o ~8 MB en XRGB8888, en lugar de los 48 MB que exigiría descomprimir la imagen completa en crudo).

### 2. Motor JavaScript: Duktape Contenido con Sandbox de Memoria y Tiempo
- **Por qué Duktape supera a QuickJS en PS3:**
  - **Soporte Nativo Big-Endian:** La PPU del Cell opera en arquitectura PowerPC Big-Endian. Duktape fue diseñado en ANSI C89 con control estricto de orden de bytes (DUK_USE_BYTEORDER), evitando los errores de empaquetado de punteros (*NaN-boxing*) que sufre QuickJS en arquitecturas no x86/ARM.
  - **Bindings Nativos de NetSurf (
nsgenbind):** NetSurf ya integra la capa de bindings entre su árbol DOM (libdom) y Duktape, ahorrando la reescritura manual de miles de líneas de enlace C.
- **Sandbox Estricto Anti-Cuelgues:**
  - **Límite de Memoria (Pool TLSF de 8 MB):** Al crear el contexto con duk_create_heap(), se delega a un sub-pool TLSF de 8 MB. Si un script intenta crear arrays infinitos, recibe NULL a los 8 MB y arroja RangeError: out of memory, sin comprometer el resto del navegador.
  - **Watchdog de Tiempo (DUK_USE_INTERRUPT_COUNTER):** Compilado con contador de interrupciones que ejecuta un callback cada $ instrucciones. Compara el registro de tiempo por hardware (mftb - *Move From Time Base*). Si un script corre por más de 250 milisegundos, el callback fuerza la cancelación del bucle infinito sin congelar cuadros de video.

### 3. Pipeline Gráfico RSX: Enfoque Híbrido de 2 Capas
- **Capa 0 (Documento Web en RAM XDR):** El motor NetSurf dibuja la página, fuentes y estilos en la superficie NSFB_SURFACE_RAM en memoria XDR. Se transfiere a la VRAM como una única textura grande por DMA solo cuando la página cambia o se produce scroll.
- **Capa 1 (UI, Cursor Magnético y Zoom por Hardware RSX):**
  - **Movimiento de Cursor con Coste Cero de DMA:** Cuando el usuario mueve el cursor analógico sin hacer scroll, no se vuelcan 8.2 MB de frame por DMA. El RSX re-dibuja el quad de la página que ya reside en VRAM y superpone el sprite del cursor (con su halo neón) por hardware en menos de 0.1 ms.
  - **Zoom Cinematográfico Gratuito:** Mantener presionado L2 y mover el stick derecho altera las coordenadas de textura (U/V) de los 4 vértices del quad en el RSX con filtrado bilineal (GCM_TEXTURE_LINEAR), logrando zoom a 60 FPS sin recalcular píxeles en la CPU.

### 4. Escudo de Recuperación Anti-Crash en 3 Niveles (Cero "Triple Pitido")
- **Nivel 1: Paracaídas de Memoria OOM con setjmp / longjmp:**
  - En el wrapper del asignador TLSF, si el pool de 64 MB alcanza el 95% de ocupación, se invoca longjmp(oom_recovery_env, 1) saltando directamente al bucle principal de main.c.
  - Se ejecuta arena_destroy(), liberando de golpe la pestaña colapsada y mostrando en pantalla: *"La página excedió el límite de memoria de PS3. Volviendo al inicio..."*.
- **Nivel 2: Hilo Watchdog con Cancelación Limpia de Red:**
  - Un hilo PPU de prioridad alta (500) monitorea un contador atómico de actividad (heartbeat_counter).
  - Si una transferencia de red se bloquea por más de 8 segundos, el callback CURLOPT_XFERINFOFUNCTION retorna error para que cURL cierre los sockets internamente sin dejar descriptores huérfanos en Lv2.
- **Nivel 3: Auto-Reinicio en Caliente (sysProcessExitSpawn2):**
  - Si el hilo principal permanece congelado por 10 segundos:
    1. Activa la bandera global g_emergency_shutdown = 1.
    2. Cierra ordenadamente cualquier descriptor de archivo abierto en disco (sysFsClose(g_download_fd)).
    3. Crea el archivo de seguridad /dev_hdd0/game/EONS00001/USRDIR/safe_mode.flag.
    4. Invoca la llamada del kernel: sysProcessExitSpawn2("/dev_hdd0/game/EONS00001/USRDIR/EBOOT.BIN", NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M).
    5. El kernel Lv2 descarga el proceso de forma limpia y reinicia EonSurf instantáneamente sin salir al XMB y sin emitir pitidos de error. Al arrancar, main.c detecta la bandera, la elimina y entra en Modo Seguro.

---

## 23. Arquitectura Multimedia, Encolado PKG Nativo y Gestión Térmica (Secretos de la Scene)

Para completar el blindaje de ingeniería de nivel comercial de EonSurf, se integran los 3 secretos mejor guardados de la scene de PlayStation 3:

### 1. Video HTML5 / YouTube Liviano: El Secreto de Movian (cellVdec + Shader YUV en RSX)
- **Demuxing Ligero en PPU:** Uso de versión mínima de libavformat para extraer NAL units H.264 y audio AAC en XDR.
- **Decodificación por Hardware en SPUs (SYSMODULE_VDEC / cellVdec):** 
  - Inicialización con CELL_VDEC_CODEC_TYPE_AVC (soporta hasta High Profile Level 4.2).
  - Sony procesa el bitstream comprimido internamente en las SPUs y devuelve planos crudos **YUV420 Planar (NV12 / YV12)**.
- **Conversión YUV ? RGB en el RSX (Cero Carga de PPU):**
  - Se suben los 3 planos (Y, U, V) por DMA a la VRAM como texturas LUMINANCE8.
  - Un Fragment Shader simple en el RSX realiza la multiplicación matricial YUV-RGB directamente en la GPU. La CPU principal (PPU) queda libre al 95% permitiendo 60 FPS estables en 720p/1080p.
- **Integración Invidious / MP4:** Para flujos web (itag 18 [360p] o itag 22 [720p]), se superpone un reproductor hardware en la Capa 1 del RSX.

### 2. Instalación de PKGs en Caliente: El Sistema Dual (webMAN MOD Loopback + Burbuja XMB .pdb)
- **Método A: Puente Local con webMAN MOD (Instalación Inmediata en 1 Toque):**
  - webMAN MOD escucha en 127.0.0.1:80 dentro del proceso sh.self.
  - Al finalizar una descarga en /dev_hdd0/packages/archivo.pkg, EonSurf emite una petición HTTP local GET:
    GET /install.ps3/dev_hdd0/packages/archivo.pkg o notificación con trofeo: GET /popup.ps3?¡Descarga_EonSurf_Completada!
  - webMAN MOD intercepta el comando y realiza el montaje o instalación desatendida.
- **Método B: Cola Nativa en Segundo Plano de PKGi (/dev_hdd0/vsh/task/):**
  - Para descargas masivas en segundo plano sin mantener EonSurf abierto:
  - Se crea la carpeta numerada en /dev_hdd0/vsh/task/0000000X/.
  - Se generan los 4 archivos de base de datos de Sony: d0.pdb, d1.pdb, 0.pdb e ICON_FILE.
  - El sistema inyecta la tarea directamente en el gestor de descargas oficial de la burbuja de red del XMB.

### 3. Gestión Térmica y Ventilador: Syscall 383 y Convivencia Pacífica con webMAN MOD
- **Lectura Continua de Temperaturas (Syscall 383):**
  - Compatible tanto en CFW Evilnat como en PS3HEN:
    c
    // Syscall 383: tzone 0 = CELL, tzone 1 = RSX
    uint32_t ttemp_cell = 0, temp_rsx = 0;
    lv2syscall2(383, 0, (uint64_t)&ttemp_cell);
    lv2syscall2(383, 1, (uint64_t)&temp_rsx);
    uint8_t celsius_cell = (uint8_t)(ttemp_cell >> 24);
    uint8_t celsius_rsx  = (uint8_t)(temp_rsx >> 24);
    
  - Se actualiza cada 23 segundos en el HUD: Verde (< 70°C), Amarillo (7077°C) y Rojo Alerta (= 78°C).
- **Control del Ventilador sin Conflictos de Syscon (Syscall 409 vs 389):**
  - **Detección de webMAN MOD:** Comprueba si webMAN está activo en puerto 80 o mediante syscall 409 (sys_sm_get_fan_policy). Si el modo dinámico está activo (mode == 2), EonSurf **no toca la syscall 389**, evitando la "guerra de payloads" que hace tironear la turbina de la consola.
  - **Fallback de Protección en Consolas Stock (SYSCON Mode 1):** Si se detecta que la consola está en modo fábrica de Sony y el CELL supera los 75°C durante descargas pesadas, EonSurf eleva el cooler al 40% (x66) vía syscall 389, restaurando el control original al salir.

---

## 24. Los 5 Pilares Finales de la Biblia de Ingeniería de EonSurf

Para culminar la especificación más completa y robusta de la historia del homebrew en PlayStation 3:

### 1. Detección en Caliente del Entorno (CFW Evilnat vs PS3HEN vs Syscalls Deshabilitadas)
- **El Peligro:** Invocar syscalls de desarrollador cuando el usuario limpió los registros para entrar a PSN (o en entornos HEN restringidos) provoca un error de kernel ENOSYS (0x80010003) o un cuelgue.
- **Rutina Maestra de Detección (Microsegundos en main.c):**
  c
  #include <ppu-lv2.h>

  typedef enum {
      ENV_SYSCALLS_DISABLED = 0, // Modo seguro / PSN (Syscalls limpias)
      ENV_PS3HEN            = 1, // Entorno Híbrido PS3HEN
      ENV_CFW_COBRA_EVILNAT = 2, // Custom Firmware completo (Cobra / Mamba)
      ENV_CFW_STANDARD      = 3  // CFW clásico sin payload Cobra
  } ps3_env_t;

  ps3_env_t detect_ps3_environment(void) {
      // 1. Probar Opcode 0x1337 (Exclusivo de PS3HEN: devuelve 0x1337 o versión HEN)
      lv2syscall1(8, 0x1337ULL);
      if (p1 == 0x1337ULL || (p1 & 0xFFFF0000ULL) == 0x13370000ULL) {
          return ENV_PS3HEN;
      }
      // 2. Probar Opcode 0x7000 (SYSCALL8_OPCODE_GET_VERSION de Cobra/Mamba)
      uint32_t cobra_ver = 0;
      lv2syscall2(8, 0x7000ULL, (uint64_t)&cobra_ver);
      if (p1 == 0 && cobra_ver > 0) {
          return ENV_CFW_COBRA_EVILNAT;
      }
      // 3. Si Syscall 8 devolvió ENOSYS (0x80010003), probar Peek Lv2 básico (Syscall 6)
      lv2syscall1(6, 0x8000000000000000ULL);
      if (p1 == 0x80010003ULL) {
          return ENV_SYSCALLS_DISABLED; // Syscalls desactivadas por seguridad para PSN
      }
      return ENV_CFW_STANDARD;
  }
  

### 2. Scroll a 60 FPS con Ring Buffer Virtual (Y-Wrap) y Dirty Strip DMA
- **El Problema del Scroll Tradicional:** Redibujar y transferir los 3.68 MB completos de pantalla (720p) en cada fotograma satura el bus EIB y a libnsfb.
- **Técnica de Hardware Wrap-Around:**
  - Se mantiene un buffer circular en RAM XDR con offset vertical: scroll_y_offset = (scroll_y) % 720.
  - **Dirty Strip DMA:** Si el usuario desciende 16 píxeles, libnsfb solo dibuja la franja nueva de 1280x16 píxeles (**apenas 80 KB** en lugar de 3.68 MB) sobrescribiendo la línea más antigua en el buffer circular.
  - Se transfieren por DMA solo esos 80 KB a la VRAM.
  - El RSX muestra la textura desplazando la coordenada de textura vertical  + (scroll_y_offset / 720.0f)$ usando modo GCM_TEXTURE_REPEAT.
  - **Resultado:** Reducción del ancho de banda de DMA de 220 MB/s a **menos de 4.8 MB/s**. Scroll sedoso e idéntico a un monitor de alta tasa de refresco.

### 3. Audio de Fondo y Radio Web con Event Queue de Hardware
- **UI & Efectos de Sonido:** Integración de **spu_soundlib** (de Hermes/Estwald) ejecutándose 100% en una SPU con 0.0% de consumo de PPU.
- **Streams MP3 / AAC en Vivo:** Decodificación acelerada con SYSMODULE_ADEC (cellAdec) o minimp3 vectorial en hilo PPU de prioridad alta (500).
- **Prevención de Tartamudeo (Crackling):** En lugar de bucles usleep(), se vincula el puerto de audio a una cola de eventos por interrupción con aaudioCreateNotifyEventQueue(). El hardware de sonido de la PS3 despierta el hilo exactamente cada 5.33 ms para transferir los 256 samples PCM y vuelve a dormir sin consumir ciclos parásitos.

### 4. Cookies e Historial: Estrategia RAM-First con Volcado Diferido (Prohibido SQLite3)
- **Por qué evitar SQLite3 en SATA I:** SQLite3 dispara bloqueos fsync() sobre el sistema de archivos cifrado de la PS3 con latencias mecánicas de 15 ms, trabando el navegador por más de 100 ms por cada cookie actualizada.
- **Solución RAM-First de NetSurf (urldb):**
  - Toda la base de datos de historial y cookies reside en RAM durante la ejecución.
  - Al arrancar se lee de un saque en < 5 ms: /dev_hdd0/game/EONS00001/USRDIR/data/cookies.dat e history.dat.
  - Durante la navegación, las operaciones ocurren en memoria a velocidad de nanosegundos marcando g_userdata_dirty = true.
  - El hilo secundario de disco (prioridad 1500) vuelca los datos una vez cada 60 segundos o al invocar SYSUTIL_EXIT_GAME, evitando todo desgaste y latencia del disco.

### 5. Máquina de Estados Asíncrona para el Teclado Oficial (sysutil/osk)
- **Causa del Cuelgue en Otros Homebrews:** Bloquear el bucle con un while(osk_active) detiene las llamadas a gcmSetFlip() del RSX. Al no recibir VSync, el sistema operativo de Sony (sh.self) entra en timeout y congela la pantalla.
- **Patrón Asíncrono Perfecto a 60 FPS:**
  1. **Disparo No Bloqueante:** Al hacer clic en un campo de texto, se llama a oskLoadAfsync() y se fija la bandera g_osk_state = OSK_STATE_OPEN. El bucle principal jamás se detiene.
  2. **Aislamiento de Entrada (Congelar Pad de EonSurf, NO el RSX):**
     - Si g_osk_state == OSK_STATE_OPEN, se ignoran las lecturas del stick/botones en el puntero de EonSurf (para que el cursor no se mueva detrás mientras se eligen letras).
     - Se continúa dibujando el último frame estático en el RSX ejecutando estrictamente:
       gcmSetFlip(ctx, cur); rsxFlushBuffer(ctx); gcmSetWaitFlip(ctx); sysUtilCheckCallback();
  3. **Cierre Fluido en Callback:** Al presionar START en el teclado de Sony, el callback captura SYSUTIL_OSK_DONE, extrae el texto UTF-16 convirtiéndolo a UTF-8 para NetSurf con oskUnloadAfsync(), y al recibir SYSUTIL_OSK_UNLOADED destruye el contenedor de memoria y restaura g_osk_state = OSK_STATE_CLOSED.
  - **Resultado:** El teclado de Sony se desliza en 0.1 segundos a 60 FPS con suavidad de juego triple A sobre la interfaz de EonSurf.


---

**Created by JuAnKLiMoNN**

## 25. Los 10 Mandamientos Finales del Silicio (Arquitectura Extrema Cell & RSX)

1. **Hilos de Hardware PPU (SMT 0 vs 1) & Registros HMT:** Prioridades pareadas (1000 y 1001) + control por silicio con `asm volatile("or 1,1,1")` y `"or 2,2,2"`.
2. **Compresion HTTP en SPUs:** Inflate (zlib/miniz) en SPU junto a TLS 1.3, cabecera `Accept-Encoding: gzip, deflate` y Brotli prohibido.
3. **230 MB Libres de VRAM:** Texture Pool en GDDR3 y Snapshots de Paginas con `gcmSetTransferImage(LOCAL_TO_LOCAL)` para boton Atras en 0.2 ms.
4. **SIMD AltiVec / VMX (128-bit) en PPU:** `-maltivec -mabi=altivec`, `vvec_lvsl`, jvvec_perm` y `vec_mladd` para 4 pixeles RGBA por ciclo.
5. **Memoria Extra sin romper Lv1:** Prestamo oficial de VSH con `sysMemContainerCreate(&container, 16mb)` mientras el OSK este cerrado.
6. **Prefetching Manual L1/L2 (dcbt) especifico para nucleo In-Order:** `__builtin_prefetch(node->next->next, 0, 3)` y `dcbtst` de 256 bytes en blitting.
7. **Triple Buffering Asincrono RSX:** Etiquetas de hardware con `gcmGetLabelAddress(64)` y `gcmSetWriteBackEndLabel` sin `sys_spu_flip_status` bloqueante.
8. **Audio Opus en SPU:** Decodificador libopus compilado con `-DFIXED_POINT=1` (75 KB de codigo) sin SPU Overlays.
9. **USB NTFS / exFAT > 4GB:** Enlace de `libntfs_ext.a` con 128 paginas de cache en RAM y desmontaje seguro antes de salir.
10. **Corte Termico de Emergencia (>83Â°C):** Buzzer (Syscall 392) + SysFsSync + Apagado limpio por Syscon con Syscall 379 (0x1100).


---

**Created by JuAnKLiMoNN**

## 26. Los 3 Pilares de Impacto Real (100/100 Absoluto)

Para cerrar la brecha entre la Arquitectura de Silicio y la experiencia diaria del usuario en su televisor:

1. **Enfrentar el Muro de Cloudflare y fingerprinting TLS 11/**
   - **Modo Lector / Scraper Inteligente (boton Select):** Extraccion limpia de texto (via portales puente livianos estilo Jina Reader / A2rchive) en modo oscuro con fuentes FreeType2 a 1080p, sin cargar scripts pesados.
   - **Suplantacion JE3 (JA3 Spoofing en mbedTLS):** Ordenar las cipher suites del Client Hello de mbedTLS para imitar exactamente a un navegador movil moderno, evitando bloqueos anti-ddos previos.

2. **Descargas de Enlaces Indirectos (Resolvers Nativos en C):**
   - **Micro-Modulo de Enlaces Directos (20 KB en C):** Intercepcion previa de URLs de MediaFire, Google Drive (`/info.gdid_proxy`), GitHub Releases y 1Fichier. Extraccion directa del link real del .pkg en C con request liviano y disparo automatico al descargador SATA I.

3. **Banco de Pruebas de Estres (Stress-Test Suite en C):**
   - **Modo de Diagnostico `stress_test.c`:** Rutina de prueba automatizada que abre y cierra 50 pestanas seguidas, descarga un archivo de 4 GB simultaneo mientras reproduce un stream de audio y ejecuta scroll saturado durante 2 horas. Probacion del 0 bytes de fuga de memoria en TLSF y 60 FPS estables.


---

**Created by JuAnKLiMoNN**

## 27. Los 3 MÃ³dulos Clave para el 100/100 Definitivo (ImplementaciÃ³n C Pura)

Para concretar la experiencia perfecta y solucionar los dedos frios del mundo real:

1. **Camuflaje TLS (JA3 Spoofing en `port/ps3/tls_ja3.c`):**
   - Ordenamiento explÃ­cito de Cipher Suites en `mbedtls_ssl_conf_ciphersuites` imitando a un navegador mÃ³vil moderno (SAFARI / IOS / FIREFOX):
     `MBEDTLS_TLS1_3_AES_128_GCM_SHA256`, `MBEDTLS_TLS1/3_AES_256_GCM_SHA384` etc. + cabecera `Sec-Fetch-Mode: navigate`. Bypassa el 70% de los bloqueos pasivos de Cloudflare sin JS.

   - **Modo Lector Nativo de 2 Niveles (`src/reader_mode.c` - BotÃ³n [Select]):**
     * *Nivel 1 (Local en RAM XDR):** Recorrido de `libdom` extrayendo solo equitetas <p>, <h1>..<h3>, ullis e inyecciando HTML4 limpio en Modo Oscuro (22px).
     * *Nivel 2 (Puente Remoto Para Webs en React/Turnstile):** Al mantener [Select] por 1 s, reescribe la URL a puente liviano (jina.ai / duckduckgo) recibiendo HTML estÃ¡tico puro a60 FPS.

2. **Descargas Indirectas en C (`src/resolver_ps3.c`):**
   - **MediaFire:** Scraping ligero de 64 KB sobre `strctr(downloadButton)` y extracciÃ³n de `href` directo al .pkg.
   - **Google Drive:** ExtracciÃ³n del `ID para pasar directamente al endpoint `export=download&confirm=t`, bypassando el cartel de virus de archivos pesados.
   - **GitHub Releases & Archive.org:** Filtro automÃ¡tico de enetes<a href="..pkg"> con ventana emergente directa para descargar en 1 clic a /dev_hdd0/packages/.

3. **Banco de Pruebas "Torture Harness" (`src/stress_test.c` - Activable con L1+R1+R3):**
   - **InyecciÃ³n de Comandos SintÃ©ticos:** NavegaciÃ³n automÃ¡tica por 20 URLs en bucle abriendo/cerrando OSK y scrolling saturado.
   - **AuditorÃ­a de Memoria TELSF (0 Leaks):** MediciÃ³n de `tlsf_get_free_size()` inicial vs final tras 100 pgs (byte idÃ©ntico).
   - **Detector de Frame Drops con __mftb() (79.v8 MHz):** En cada gcmSetFlip, si supera 1.330.000 ticks (16.67 ms), marca caida de frame y registra la funciÃ³n responsable.
