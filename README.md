# PA3 — Laboratorio de renderizado 3D

Implementación de referencia en **C++17, OpenGL 3.3 Core y GLSL 330**, basada en las dos páginas de `PA3 - Simplificado.pdf`.

> **Uso académico:** el PDF establece que el informe evaluado no debe ser generado por IA. Este README y `ENTREGA.md` son documentación técnica de una implementación asistida por IA, no el informe del estudiante. El equipo debe verificar las reglas de uso del código, comprenderlo, realizar sus pruebas y escribir su propio informe. No se ha publicado un repositorio ni grabado el video explicativo del equipo.

## 1. Resumen de arquitectura y análisis técnico

### 1.1 Requerimientos y decisiones

El PDF no especifica ecuaciones, tipos de primitivas ni un modelo de iluminación concreto. Las siguientes son decisiones de implementación, no fórmulas atribuidas al documento.

| Requerimiento del PDF, página 1 | Implementación | Evidencia en código |
|---|---|---|
| Al menos seis objetos 3D diferenciables | Suelo con grosor, cubo, esfera, cilindro, cono y toro; seis instancias y cinco mallas compartidas | `Scene.cpp`, `Primitives.cpp` |
| Cámara para visualizar la escena | Cámara orbital, perspectiva, límites de elevación y zoom; dos posiciones predefinidas | `Camera.cpp` |
| Depth Buffer | Solicitud de 24 bits; comprobación de profundidad disponible; `GL_DEPTH_TEST`, `GL_LESS`; limpieza cada cuadro | `Window.cpp`, `Renderer.cpp` |
| Dos fuentes o condiciones de luz como mínimo | Ambiente + luz direccional + luz puntual atenuada | `Scene.h`, `scene.frag` |
| Iluminación y sombreado que revelen volumen | Blinn–Phong por fragmento, normales analíticas e interpoladas | `Primitives.cpp`, shaders |
| Materiales o colores diferenciados | Albedo, intensidad especular y exponente por objeto | `Material`, `Scene.cpp` |
| Una textura como mínimo | Damero RGB procedural de 256 × 256, subido como textura 2D y muestreado con UV en el suelo | `Texture.cpp`, `scene.frag` |
| Efecto complementario | Rotación continua del toro con pausa; no requiere modificar vértices | `Object::model`, `Scene::update` |
| C/C++ y OpenGL; modularidad recomendada | C++17, clases con responsabilidades separadas, CMake | Todo el proyecto |
| Varias pruebas de parámetros, página 2 | Escenario base y cinco variantes reproducibles; controles interactivos | `Scene::applyCase`, CTest |

**Alcance:** no se incorporan PBR, mapas de sombras, transparencia, reflexiones ni postprocesamiento: el PDF no los exige y permite explícitamente escoger animación como efecto. Blinn–Phong facilita explicar los componentes ambientales, difusos y especulares. No es un modelo PBR ni conserva energía; los valores por encima de 1 se recortan en la salida LDR. El Depth Buffer resuelve oclusión desde la cámara, **no** sombras desde las luces.

El suelo es un paralelepípedo 3D, no un fondo de pantalla, por lo que forma parte de los seis objetos. El cubo y el suelo reutilizan la misma malla con distintas transformaciones y materiales. El toro flota para poder rotar sin atravesar el suelo.

### 1.2 Componentes y flujo de datos

```text
GLFW/eventos → Window/entrada → Camera + Scene
Primitives → MeshData CPU → Mesh: VAO + VBO + EBO GPU
Material + Lighting + matrices → Shader: uniforms
Damero CPU → Texture GL_SRGB8 + mipmaps GPU
Scene + Camera → Renderer → vertex shader → rasterización
    → fragment shader → prueba de profundidad → framebuffer → SwapBuffers
```

- `Window`: vida de GLFW/contexto, ventana, estado del teclado, capacidades y errores OpenGL. Usa `std::unique_ptr` con destructor personalizado para `GLFWwindow`.
- `GlResource`: nombre GPU con propiedad exclusiva, destructor RAII, copia prohibida y movimiento `noexcept`.
- `Shader`: compilación, enlace, registro de errores, uniforms tipados y caché de ubicaciones. Un uniform requerido inexistente es un error explícito.
- `Mesh`: atributos intercalados de posición/normal/UV e índices de 32 bits; carga estática y dibujo indexado.
- `Primitives`: generación CPU de cubo, esfera UV, cilindro cerrado, cono cerrado y toro; UV duplicadas en costuras y normales independientes en tapas/aristas.
- `Texture`: textura procedural real en GPU, repetición, filtrado trilineal y mipmaps; sin archivos de imagen externos.
- `Camera`: vista orbital y perspectiva, ajustadas al tamaño real del framebuffer.
- `Material`, `Object`, `Lighting`, `Scene`: datos y actualización de la escena; acceso a mallas por índice estable.
- `Renderer`: estado por cuadro, estado por objeto y llamadas de dibujo; captura PPM opcional.

No se introduce `Model` porque no hay importación de modelos ni jerarquías; `Object` representa una instancia con transformación/material y referencia a una malla. Un importador futuro puede producir `MeshData` y objetos sin cambiar el shader o la cámara. No se usa `shared_ptr` porque no existe necesidad de propiedad compartida: la escena es la propietaria de las mallas y las instancias solo guardan índices.

### 1.3 Modelo matemático

Se usan vectores columna y matrices GLM; las matrices se envían con `transpose = GL_FALSE`.

**Transformaciones:**

\[
M=T\,R\,S,\qquad p_w=M p_o,\qquad p_{clip}=P\,V\,M\,p_o.
\]

`T R S` aplica primero escala, después rotación y finalmente traslación. La vista es `lookAt(eye,target,up)` y la proyección tiene FOV vertical de 45°, plano cercano 0.1 y lejano 100. Se evita un plano cercano excesivamente pequeño para conservar precisión de profundidad.

**Normales:**

\[
N_w=\operatorname{normalize}\!\left((M_{3\times3}^{-1})^T N_o\right).
\]

La inversa transpuesta se calcula una vez por objeto en CPU. Se renormaliza por fragmento después de interpolar. Todas las escalas de esta escena son positivas y no nulas; una extensión que permita escalas nulas deberá rechazarlas y una escala negativa exigirá considerar la inversión del winding.

**Iluminación en coordenadas mundiales:**

\[
V=\widehat{p_{eye}-p_w},\quad L_d=\widehat{-d_{light}},\quad
L_p=\widehat{p_{light}-p_w},\quad H_j=\widehat{L_j+V}.
\]

\[
f_{att}(d)=\frac{1}{k_c+k_l d+k_qd^2},\qquad (k_c,k_l,k_q)=(1,0.09,0.032).
\]

\[
C_{lin}=A\odot a+
\sum_{j\in\{d,p\}}w_j I_j\odot
\left[a\max(N\cdot L_j,0)+
k_s\,\mathbf{1}_{N\cdot L_j>0}\max(N\cdot H_j,0)^s\right].
\]

Aquí `a` es el albedo lineal del material multiplicado por la textura si está activa; `w_d=1`, `w_p=f_att`, `A` es ambiente, `I_j` el color/intensidad de la fuente, `k_s` la intensidad especular y `s` el exponente. Se protege la normalización de vectores de longitud cero. La condición `N·L>0` evita brillos especulares en superficies no iluminadas.

**Espacio de color:** la textura usa `GL_SRGB8`, por lo que el muestreo devuelve RGB lineal. Los albedos del código ya son lineales. Al final se aplica la función sRGB por tramos a la salida limitada al rango [0,1]. `GL_FRAMEBUFFER_SRGB` está desactivado para no aplicar gamma dos veces.

**Geometría:**

- Esfera unitaria: `p(θ,φ)=(sinφ cosθ, cosφ, sinφ sinθ)`, normal igual a `p`.
- Cilindro: radio 1, `y∈[-1,1]`, normal lateral `(cosθ,0,sinθ)`.
- Cono: radio de base 1, altura 2; normal lateral proporcional a `(cosθ,1/2,sinθ)`.
- Toro: `p=((R+r cosφ)cosθ,r sinφ,(R+r cosφ)sinθ)`, `R=0.8`, `r=0.28`.
- Se excluyen triángulos degenerados de los polos, se orientan las caras CCW y se separan las normales de caras duras. Las costuras duplicadas evitan interpolar UV entre 0 y 1 atravesando toda la textura.

**Animación:** `θ(t+Δt)=(θ(t)+0.7Δt) mod 2π`. En modo interactivo se limita `Δt` a 0.05 s tras pausas largas; en ejecución limitada por `--frames` se usa 1/60 s fijo para reproducibilidad.

### 1.4 Optimización, memoria y robustez

- Cinco mallas GPU para seis objetos, sin buffers nuevos por cuadro. Los temporales CPU de geometría se liberan después de construir cada `Mesh`.
- Un VAO, un VBO intercalado y un EBO por malla; `GL_STATIC_DRAW` y `glDrawElements`.
- Un programa shader y una textura compartida; uniforms de cámara y luces una vez por cuadro, material y matrices por objeto.
- `glGetUniformLocation` solo en la primera consulta de cada nombre. No se recompilan shaders durante el bucle.
- Culling de caras traseras, profundidad y mipmaps. No se fuerza early-Z mediante extensiones: su aplicación concreta depende del driver.
- Se usa `sampler2D`; un texture buffer object no aporta ventajas para una imagen 2D. Tampoco se justifican UBO, SSBO, instancing, render diferido o multihilo con seis objetos. Para muchas instancias del mismo mesh, el siguiente paso sería agrupar por material y usar instancing.
- Los nombres GPU se liberan automáticamente incluso si falla un constructor; shaders temporales también usan RAII. `Renderer` y `Scene` se destruyen antes de `Window`, conservando un contexto activo.
- Callback GLFW; comprobación de GLEW, profundidad y versión; logs GLSL de compilación/enlace; validación de índices y resolución de primitivas; tratamiento de errores de captura y línea de comandos.
- `glDebugMessageCallback` se registra cuando existe OpenGL 4.3 o `KHR_debug`. OpenGL 3.3 no lo garantiza; se conserva `glGetError` como verificación al inicializar, al terminar y en cada cuadro. El callback no arroja excepciones a través del driver. La comprobación por cuadro prioriza diagnóstico didáctico sobre rendimiento máximo.
- Minimizar la ventana no causa división por cero ni un bucle de render ocupado. La relación de aspecto se obtiene del framebuffer, no del tamaño lógico de ventana.
- GLSL se conserva en archivos `.vert` y `.frag`, pero CMake lo incorpora a un encabezado generado durante la configuración. No se depende del directorio de trabajo ni de copiar assets junto al ejecutable. Cambiar un shader provoca reconfiguración/recompilación.

## 2. Estructura del proyecto

```text
PA3/
├── CMakeLists.txt
├── vcpkg.json
├── .gitignore
├── README.md
├── ENTREGA.md                  # Esta guía + todos los archivos de código
├── cmake/
│   └── ShaderSources.h.in
├── include/pa3/
│   ├── GlResource.h
│   ├── Window.h
│   ├── Shader.h
│   ├── Mesh.h
│   ├── Primitives.h
│   ├── Texture.h
│   ├── Camera.h
│   ├── Scene.h
│   └── Renderer.h
├── src/
│   ├── main.cpp
│   ├── Window.cpp
│   ├── Shader.cpp
│   ├── Mesh.cpp
│   ├── Primitives.cpp
│   ├── Texture.cpp
│   ├── Camera.cpp
│   ├── Scene.cpp
│   └── Renderer.cpp
├── shaders/
│   ├── scene.vert
│   └── scene.frag
└── docs/
    ├── scene.png               # Captura real con Mesa/llvmpipe
    └── validation/             # Registros de compilación y pruebas locales
```

## 3. Instrucciones de configuración

### 3.1 Windows y Visual Studio 2022

Requisitos: Windows x64, Visual Studio 2022 con **Desarrollo para el escritorio con C++**, MSVC v143, Windows SDK y herramientas CMake para C++; Git; CMake 3.21 o posterior; driver gráfico del fabricante con OpenGL 3.3 Core. No basta el driver OpenGL genérico antiguo de Windows.

Dependencias: **GLFW**, **GLEW**, **GLM** y OpenGL del sistema. El manifiesto vcpkg instala las tres bibliotecas. GLM es de encabezados; CMake resuelve includes, bibliotecas y definiciones de GLEW. No se debe incluir `gl.h` antes que GLEW. No se necesita GLAD ni GLU para el código de esta aplicación.

En PowerShell, instalar vcpkg una sola vez (si ya está instalado, usar su ubicación):

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
& C:\dev\vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = 'C:\dev\vcpkg'
```

Abrir PowerShell en la carpeta `PA3` extraída y ejecutar:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release
.\build\Release\PA3Scene.exe
```

El triplet estático evita distribuir DLL de GLFW/GLEW junto al ejecutable. La primera configuración requiere Internet para descargar dependencias. Para usar otro triplet, crear un directorio de compilación nuevo; no mezclar configuraciones estáticas y dinámicas en la misma caché.

Para trabajar desde el IDE:

```powershell
Start-Process .\build\PA3Scene.sln
```

Seleccionar `PA3Scene` como proyecto de inicio si no se selecciona automáticamente; `Release | x64`; ejecutar con Ctrl+F5. Para depuración, compilar con `--config Debug` y seleccionar `Debug | x64`.

Pruebas y captura:

```powershell
ctest --test-dir build -C Release --output-on-failure
.\build\Release\PA3Scene.exe --frames 3 --case 4 --capture sin-textura.ppm
```

Las pruebas necesitan una sesión gráfica disponible y un contexto OpenGL real; no son pruebas puramente CPU. PPM se abre con aplicaciones como GIMP o ImageMagick. `docs/scene.png` ya ofrece una captura en PNG. La captura se lee antes de intercambiar los buffers y se invierte verticalmente al escribirla.

**Vinculación explícita en CMake:** `OpenGL::GL`, `glfw`, `GLEW::GLEW`, `glm::glm`. Se habilita C++17, sin extensiones de lenguaje, y en MSVC `/W4 /permissive- /utf-8 /EHsc`. Se definen `GLFW_INCLUDE_NONE`, `GLM_FORCE_RADIANS` y `NOMINMAX`.

El manifiesto no fija un baseline de vcpkg. Para congelar un entorno de entrega, registrar el commit de vcpkg utilizado (`git -C C:\dev\vcpkg rev-parse HEAD`) y conservarlo con la documentación del equipo. No se afirma reproducibilidad binaria entre revisiones de dependencias.

### 3.2 Linux — entorno usado para validar

```bash
sudo apt-get install cmake ninja-build g++ libgl1-mesa-dev libglfw3-dev libglew-dev libglm-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/PA3Scene
ctest --test-dir build --output-on-failure
```

En una máquina sin monitor, instalar `xvfb` y `xauth`, y ejecutar `xvfb-run -a ctest --test-dir build --output-on-failure`. No configurar el toolchain vcpkg si se usan los paquetes de la distribución.

### 3.3 Controles y experimentación

| Tecla | Acción |
|---|---|
| Flechas | Órbita horizontal/vertical |
| W / S | Acercar / alejar |
| A / D | Mover luz puntual en −X / +X |
| Q / E | Mover luz puntual en −Y / +Y |
| I / K | Mover luz puntual en −Z / +Z |
| T | Activar/desactivar textura del suelo |
| B | Activar/desactivar componente especular |
| P / L | Activar/desactivar luz puntual / direccional |
| [ / ] | Reducir/aumentar exponente de brillo de la esfera |
| Espacio | Pausar/reanudar animación |
| R | Restablecer escena y cámara |
| 0–5 | Seleccionar experimento predefinido |
| Esc | Salir |

El título de la ventana muestra estados y posición de la luz; los experimentos también se registran en consola. No hay controles con ratón. Los símbolos `[` y `]` dependen de la distribución física del teclado; el caso 3 permite cambiar el exponente sin esas teclas.

Cada caso restablece los demás parámetros para aislar la variable. `--case N --frames 3 --capture archivo.ppm` captura una comparación temporalmente reproducible. Interactivamente, pulsar Espacio tras seleccionar un caso si se desea inspección estática.

| Caso | Parámetro modificado frente al caso 0 | Aspecto que el equipo debe inspeccionar |
|---|---|---|
| 0 | Base | Referencia |
| 1 | Luz puntual: (−3,4,3) → (4,2,−3) | Orientación de iluminación y brillos; atenuación por distancia |
| 2 | Albedo de la esfera: azul → rojo | Color difuso manteniendo geometría y luces |
| 3 | Exponente de la esfera: 96 → 8 | Anchura del brillo especular |
| 4 | Textura desactivada | Variación espacial del albedo del suelo |
| 5 | Segunda posición orbital | Perspectiva, oclusión y cambios del especular dependientes de la vista |

Esta tabla es un protocolo de comprobación, no las observaciones del informe. El equipo debe guardar evidencia propia, explicar lo observado y distinguirlo de lo esperado.

### 3.4 Validación realizada y límites

- Compilación limpia con GCC 14.2, CMake 3.31.6, GLEW 2.2, GLFW 3.4 y GLM 0.9.9.8 en Linux; sin advertencias de compilación con `-Wall -Wextra -Wpedantic`.
- Ejecución con Mesa 25.0.7 / llvmpipe, contexto Core 4.5, Depth Buffer de 24 bits y callback activo.
- **6/6 pruebas CTest aprobadas**: creación de contexto, compilación/enlace reales de GLSL, carga GPU, tres cuadros por caso, comprobación de errores y destrucción de recursos.
- **6/6 pruebas adicionales** con Mesa limitado a OpenGL 3.3 y GLSL 330 mediante `MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330`.
- Ejecución de 120 cuadros desde un directorio distinto al del proyecto: assets incorporados correctamente.
- Comparación de capturas RGB: los cinco casos modificados producen imágenes distintas de la referencia; cantidades de píxeles distintos en `docs/validation/image-differences.txt`. Esto no es una métrica de calidad visual ni sustituye inspección humana.
- `docs/scene.png` es una captura real de este programa, no una ilustración generada.
- **No se ejecutó Visual Studio/MSVC en este entorno Linux**. El proyecto incluye configuración compatible, pero debe validarse en el Windows de destino. Tampoco se automatizaron todas las interacciones del teclado, DPI, minimización ni todos los drivers de hardware.
- La ruta sin `KHR_debug` está implementada, pero no pudo aislarse en Mesa: el driver mantuvo esa extensión incluso con el contexto limitado a 3.3. No se afirma haber probado esa rama.
- Puede aparecer una advertencia del entorno Linux sobre `XDG_RUNTIME_DIR`; las ejecuciones realizadas mediante Xvfb usaron X11 y terminaron correctamente. Los registros se conservan sin ocultarla.

### 3.5 Entregables académicos pendientes del equipo

La página 2 pide repositorio, video corto explicando la iluminación e informe de autoría del equipo. El ZIP es código fuente, **no un enlace a un repositorio publicado**. Para preparar un repositorio local:

```bash
git init
git add .
git commit -m "Escena PA3: iluminacion, materiales, textura y animacion"
```

Después, crear un repositorio en la cuenta del equipo, asociar su URL y publicarlo con las credenciales propias. No se ha inventado una URL ni se ha publicado contenido sin autorización. El equipo debe grabar el funcionamiento, explicar las tres contribuciones de luz y redactar su informe conforme a la restricción del PDF.

## 4. Código fuente

Los archivos completos están en `include/pa3`, `src`, `shaders` y `cmake`. **`ENTREGA.md` reproduce todos los archivos de configuración y código en bloques separados por nombre**, sin omisiones ni marcadores de implementación pendiente. Se debe compilar el árbol de archivos, no copiar la guía entera a un único `.cpp`.
