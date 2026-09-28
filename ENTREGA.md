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

## 4. Código fuente completo

### `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.21)
project(PA3Scene VERSION 1.0 LANGUAGES CXX)
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene.vert" VERTEX_SOURCE)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene.frag" FRAGMENT_SOURCE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    shaders/scene.vert shaders/scene.frag)
configure_file(cmake/ShaderSources.h.in generated/ShaderSources.h @ONLY)
add_executable(PA3Scene src/main.cpp src/Window.cpp src/Shader.cpp
    src/Mesh.cpp src/Primitives.cpp src/Texture.cpp src/Camera.cpp
    src/Scene.cpp src/Renderer.cpp)
target_compile_features(PA3Scene PRIVATE cxx_std_17)
set_target_properties(PA3Scene PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(PA3Scene PRIVATE include "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(PA3Scene PRIVATE GLFW_INCLUDE_NONE GLM_FORCE_RADIANS NOMINMAX)
target_link_libraries(PA3Scene PRIVATE OpenGL::GL glfw GLEW::GLEW glm::glm)
if(MSVC)
    target_compile_options(PA3Scene PRIVATE /W4 /permissive- /utf-8 /EHsc)
else()
    target_compile_options(PA3Scene PRIVATE -Wall -Wextra -Wpedantic)
endif()
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT PA3Scene)
include(CTest)
if(BUILD_TESTING)
    foreach(CASE RANGE 0 5)
        add_test(NAME scene_${CASE} COMMAND PA3Scene --frames 3 --case ${CASE})
    endforeach()
endif()
```

### `vcpkg.json`

```json
{
  "name": "pa3-scene",
  "version-string": "1.0.0",
  "dependencies": ["glfw3", "glew", "glm"]
}
```

### `.gitignore`

```text
/build/
/vcpkg_installed/
/.vs/
*.ppm
```

### `cmake/ShaderSources.h.in`

```cpp
#pragma once
namespace pa3::sources {
inline constexpr const char* vertex = R"GLSL(@VERTEX_SOURCE@)GLSL";
inline constexpr const char* fragment = R"GLSL(@FRAGMENT_SOURCE@)GLSL";
}
```

### `include/pa3/Camera.h`

```cpp
#pragma once
#include "Window.h"
#include <glm/glm.hpp>
namespace pa3 {
/** Orbit camera in radians, with bounded pitch and radius to avoid singular views. */
class Camera {
public:
    void update(const Window& window, float dt);
    void preset(int index);
    glm::vec3 position() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
private:
    glm::vec3 target_{0.f, .5f, 0.f};
    float yaw_ = .55f, pitch_ = .55f, distance_ = 15.f;
};
}
```

### `include/pa3/GlResource.h`

```cpp
#pragma once
#include <GL/glew.h>
#include <stdexcept>
#include <utility>

namespace pa3 {
/** Unique ownership of a GL name. A current context must outlive this object.
 * No copying; moving transfers ownership and leaves a harmless empty object.
 */
class GlResource {
public:
    enum class Kind { Buffer, VertexArray, Texture, Program, Shader };
    explicit GlResource(Kind kind, GLenum stage = GL_VERTEX_SHADER) : kind_(kind) {
        switch (kind_) {
        case Kind::Buffer: glGenBuffers(1, &id_); break;
        case Kind::VertexArray: glGenVertexArrays(1, &id_); break;
        case Kind::Texture: glGenTextures(1, &id_); break;
        case Kind::Program: id_ = glCreateProgram(); break;
        case Kind::Shader: id_ = glCreateShader(stage); break;
        }
        if (!id_) throw std::runtime_error("Cannot allocate OpenGL resource");
    }
    ~GlResource() { reset(); }
    GlResource(const GlResource&) = delete;
    GlResource& operator=(const GlResource&) = delete;
    GlResource(GlResource&& rhs) noexcept
        : kind_(rhs.kind_), id_(std::exchange(rhs.id_, 0)) {}
    GlResource& operator=(GlResource&& rhs) noexcept {
        if (this != &rhs) {
            reset(); kind_ = rhs.kind_; id_ = std::exchange(rhs.id_, 0);
        }
        return *this;
    }
    GLuint get() const noexcept { return id_; }
private:
    void reset() noexcept {
        if (!id_) return;
        switch (kind_) {
        case Kind::Buffer: glDeleteBuffers(1, &id_); break;
        case Kind::VertexArray: glDeleteVertexArrays(1, &id_); break;
        case Kind::Texture: glDeleteTextures(1, &id_); break;
        case Kind::Program: glDeleteProgram(id_); break;
        case Kind::Shader: glDeleteShader(id_); break;
        }
        id_ = 0;
    }
    Kind kind_;
    GLuint id_ = 0;
};
}
```

### `include/pa3/Mesh.h`

```cpp
#pragma once
#include "GlResource.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
namespace pa3 {
struct Vertex { glm::vec3 position; glm::vec3 normal; glm::vec2 uv; };
struct MeshData { std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; };
class Mesh {
public:
    explicit Mesh(const MeshData& data);
    void draw() const;
private:
    GlResource vao_{GlResource::Kind::VertexArray};
    GlResource vbo_{GlResource::Kind::Buffer};
    GlResource ebo_{GlResource::Kind::Buffer};
    GLsizei count_ = 0;
};
}
```

### `include/pa3/Primitives.h`

```cpp
#pragma once
#include "Mesh.h"
namespace pa3::primitives {
MeshData cube();
MeshData sphere(unsigned slices = 48, unsigned stacks = 24);
MeshData cylinder(bool cone = false, unsigned slices = 48);
MeshData torus(unsigned rings = 64, unsigned sides = 24);
}
```

### `include/pa3/Renderer.h`

```cpp
#pragma once
#include "Shader.h"
#include "Texture.h"
#include "Scene.h"
namespace pa3 {
class Renderer {
public:
    Renderer();
    void draw(const Scene& scene, const Camera& camera, int width, int height);
    void capture(const std::string& path, int width, int height) const;
private:
    Shader shader_;
    Texture checker_;
};
}
```

### `include/pa3/Scene.h`

```cpp
#pragma once
#include "Mesh.h"
#include "Camera.h"
#include <string>
namespace pa3 {
struct Material {
    glm::vec3 albedo{1.f}; // Linear RGB, not sRGB UI colors.
    float specular = .4f;
    float shininess = 48.f;
    bool textured = false;
    float uvScale = 1.f;
};
struct Object {
    std::string name;
    std::size_t mesh = 0;
    glm::vec3 position{0.f}, scale{1.f};
    Material material;
    bool animated = false;
    glm::mat4 model(float angle) const;
};
struct Lighting {
    glm::vec3 ambient{.10f};
    glm::vec3 direction{-.6f,-1.f,-.4f}; // Direction in which light rays travel.
    glm::vec3 directionalColor{.65f,.72f,.85f};
    glm::vec3 pointPosition{-3.f,4.f,3.f};
    glm::vec3 pointColor{1.f,.68f,.40f};
    glm::vec3 attenuation{1.f,.09f,.032f};
};
class Scene {
public:
    Scene();
    void input(Window& window, Camera& camera, float dt);
    void update(float dt);
    void applyCase(int index, Camera& camera);
    std::string status() const;
    std::vector<Mesh> meshes;
    std::vector<Object> objects;
    Lighting lighting;
    bool textureEnabled = true, specularEnabled = true;
    bool pointEnabled = true, directionalEnabled = true, animate = true;
    float angle = 0.f;
private:
    void reset(Camera& camera);
};
}
```

### `include/pa3/Shader.h`

```cpp
#pragma once
#include "GlResource.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
namespace pa3 {
class Shader {
public:
    Shader(const char* vertex, const char* fragment);
    void use() const { glUseProgram(program_.get()); }
    void set(const std::string& name, int value);
    void set(const std::string& name, float value);
    void set(const std::string& name, const glm::vec3& value);
    void set(const std::string& name, const glm::mat3& value);
    void set(const std::string& name, const glm::mat4& value);
private:
    GLint location(const std::string& name);
    GlResource program_{GlResource::Kind::Program};
    std::unordered_map<std::string, GLint> locations_;
};
}
```

### `include/pa3/Texture.h`

```cpp
#pragma once
#include "GlResource.h"
namespace pa3 {
class Texture {
public:
    Texture();
    void bind() const { glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, texture_.get()); }
private:
    GlResource texture_{GlResource::Kind::Texture};
};
}
```

### `include/pa3/Window.h`

```cpp
#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <memory>
#include <utility>

namespace pa3 {
class GlfwSession {
public:
    GlfwSession();
    ~GlfwSession();
    GlfwSession(const GlfwSession&) = delete;
    GlfwSession& operator=(const GlfwSession&) = delete;
};
struct WindowDeleter {
    void operator()(GLFWwindow* window) const noexcept { glfwDestroyWindow(window); }
};
class Window {
public:
    Window();
    GLFWwindow* native() const noexcept { return handle_.get(); }
    bool down(int key) const;
    bool pressed(int key);
    std::pair<int, int> framebuffer() const;
    void checkErrors() const;
private:
    // Destruction is reversed: destroy window before terminating GLFW.
    GlfwSession session_;
    std::unique_ptr<GLFWwindow, WindowDeleter> handle_;
    std::array<bool, GLFW_KEY_LAST + 1> previous_{};
};
}
```

### `src/Camera.cpp`

```cpp
#include "pa3/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
namespace pa3 {
void Camera::update(const Window& window, float dt) {
    yaw_ += (static_cast<float>(window.down(GLFW_KEY_RIGHT))-static_cast<float>(window.down(GLFW_KEY_LEFT)))*dt;
    pitch_ += (static_cast<float>(window.down(GLFW_KEY_UP))-static_cast<float>(window.down(GLFW_KEY_DOWN)))*dt;
    distance_ += (static_cast<float>(window.down(GLFW_KEY_S))-static_cast<float>(window.down(GLFW_KEY_W)))*6.f*dt;
    pitch_ = std::clamp(pitch_, .12f, 1.4f);
    distance_ = std::clamp(distance_, 5.f, 28.f);
    yaw_ = std::remainder(yaw_, glm::two_pi<float>());
}
void Camera::preset(int index) {
    yaw_ = index == 0 ? .55f : -.85f;
    pitch_ = index == 0 ? .55f : .8f;
    distance_ = index == 0 ? 15.f : 13.f;
}
glm::vec3 Camera::position() const {
    return target_ + distance_*glm::vec3(std::cos(pitch_)*std::sin(yaw_),std::sin(pitch_),std::cos(pitch_)*std::cos(yaw_));
}
glm::mat4 Camera::view() const { return glm::lookAt(position(), target_, glm::vec3(0,1,0)); }
glm::mat4 Camera::projection(float aspect) const {
    if (aspect <= 0.f) throw std::runtime_error("Invalid camera aspect ratio");
    return glm::perspective(glm::radians(45.f), aspect, .1f, 100.f);
}
}
```

### `src/Mesh.cpp`

```cpp
#include "pa3/Mesh.h"
#include <cstddef>
#include <limits>
#include <type_traits>
namespace pa3 {
Mesh::Mesh(const MeshData& data) {
    static_assert(std::is_standard_layout_v<Vertex>);
    static_assert(sizeof(std::uint32_t) == sizeof(GLuint));
    if (data.vertices.empty() || data.indices.empty() || data.indices.size() % 3 != 0 ||
        data.indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::runtime_error("Invalid indexed triangle mesh");
    for (auto index : data.indices)
        if (index >= data.vertices.size()) throw std::runtime_error("Mesh index out of range");
    count_ = static_cast<GLsizei>(data.indices.size());
    glBindVertexArray(vao_.get());
    glBindBuffer(GL_ARRAY_BUFFER, vbo_.get());
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
                 data.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.get());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                 data.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void Mesh::draw() const {
    glBindVertexArray(vao_.get());
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
}
```

### `src/Primitives.cpp`

```cpp
#include "pa3/Primitives.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
namespace pa3::primitives {
namespace {
constexpr float pi = glm::pi<float>();
void triangle(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    // Match CCW winding to analytical outward normals. Skip zero-area pole triangles.
    const auto& va = mesh.vertices[a]; const auto& vb = mesh.vertices[b]; const auto& vc = mesh.vertices[c];
    const glm::vec3 cross = glm::cross(vb.position - va.position, vc.position - va.position);
    if (glm::dot(cross, cross) < 1e-14f) return;
    if (glm::dot(cross, va.normal + vb.normal + vc.normal) < 0.f) std::swap(b, c);
    mesh.indices.insert(mesh.indices.end(), {a, b, c});
}
void resolution(unsigned a, unsigned b = 3) {
    if (a < 3 || b < 3 || a > 512 || b > 512)
        throw std::runtime_error("Primitive resolution must be in [3,512]");
}
}
MeshData cube() {
    MeshData m;
    const glm::vec3 normals[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (const auto& n : normals) {
        const glm::vec3 helper = std::abs(n.y) > .5f ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
        const glm::vec3 u = glm::normalize(glm::cross(helper, n));
        const glm::vec3 v = glm::cross(n, u);
        const auto start = static_cast<std::uint32_t>(m.vertices.size());
        for (const glm::vec2 uv : {glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)})
            m.vertices.push_back({n*.5f + (uv.x-.5f)*u + (uv.y-.5f)*v, n, uv});
        triangle(m, start, start+1, start+2); triangle(m, start, start+2, start+3);
    }
    return m;
}
MeshData sphere(unsigned slices, unsigned stacks) {
    resolution(slices, stacks); MeshData m;
    for (unsigned j = 0; j <= stacks; ++j) {
        const float v = static_cast<float>(j)/static_cast<float>(stacks);
        for (unsigned i = 0; i <= slices; ++i) {
            const float u = static_cast<float>(i)/static_cast<float>(slices);
            const glm::vec3 n(std::sin(pi*v)*std::cos(2*pi*u), std::cos(pi*v), std::sin(pi*v)*std::sin(2*pi*u));
            m.vertices.push_back({n, n, {u, 1-v}});
        }
    }
    for (unsigned j = 0; j < stacks; ++j) for (unsigned i = 0; i < slices; ++i) {
        const unsigned a = j*(slices+1)+i, b = a+slices+1;
        triangle(m,a,b,a+1); triangle(m,a+1,b,b+1);
    }
    return m;
}
MeshData cylinder(bool cone, unsigned slices) {
    resolution(slices); MeshData m;
    // Radius=1, height=2. Cone lateral normal follows implicit gradient (x, 1/2, z).
    for (unsigned i = 0; i <= slices; ++i) {
        const float u = static_cast<float>(i)/static_cast<float>(slices);
        const float c = std::cos(2*pi*u), s = std::sin(2*pi*u);
        const glm::vec3 normal = glm::normalize(glm::vec3(c, cone ? .5f : 0.f, s));
        m.vertices.push_back({{c,-1,s}, normal, {u,0}});
        m.vertices.push_back({{cone ? 0.f : c,1,cone ? 0.f : s}, normal, {u,1}});
    }
    for (unsigned i = 0; i < slices; ++i) {
        triangle(m,2*i,2*i+2,2*i+1);
        if (!cone) triangle(m,2*i+1,2*i+2,2*i+3);
    }
    // Separate cap vertices preserve hard normal discontinuities at the rim.
    for (int cap = 0; cap < (cone ? 1 : 2); ++cap) {
        const float y = cap == 0 ? -1.f : 1.f;
        const auto center = static_cast<std::uint32_t>(m.vertices.size());
        m.vertices.push_back({{0,y,0},{0,y,0},{.5f,.5f}});
        for (unsigned i = 0; i <= slices; ++i) {
            const float a = 2*pi*static_cast<float>(i)/static_cast<float>(slices);
            const float c = std::cos(a), s = std::sin(a);
            m.vertices.push_back({{c,y,s},{0,y,0},{.5f+.5f*c,.5f+.5f*s}});
        }
        for (unsigned i = 0; i < slices; ++i) triangle(m,center,center+i+1,center+i+2);
    }
    return m;
}
MeshData torus(unsigned rings, unsigned sides) {
    resolution(rings, sides); MeshData m;
    // Major radius 0.8; tube radius 0.28. Seam duplicates carry distinct UVs.
    for (unsigned i = 0; i <= rings; ++i) for (unsigned j = 0; j <= sides; ++j) {
        const float u = static_cast<float>(i)/static_cast<float>(rings), v = static_cast<float>(j)/static_cast<float>(sides);
        const float a = 2*pi*u, b = 2*pi*v;
        const glm::vec3 n(std::cos(a)*std::cos(b),std::sin(b),std::sin(a)*std::cos(b));
        const glm::vec3 p = glm::vec3(.8f*std::cos(a),0,.8f*std::sin(a)) + .28f*n;
        m.vertices.push_back({p,n,{u,v}});
    }
    for (unsigned i = 0; i < rings; ++i) for (unsigned j = 0; j < sides; ++j) {
        const unsigned a = i*(sides+1)+j, b = a+sides+1;
        triangle(m,a,b,a+1); triangle(m,a+1,b,b+1);
    }
    return m;
}
}
```

### `src/Renderer.cpp`

```cpp
#include "pa3/Renderer.h"
#include "ShaderSources.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <fstream>
namespace pa3 {
Renderer::Renderer() : shader_(sources::vertex, sources::fragment) {}
void Renderer::draw(const Scene& scene, const Camera& camera, int width, int height) {
    if (width <= 0 || height <= 0) return;
    glViewport(0,0,width,height);
    glClearColor(.055f,.072f,.105f,1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader_.use();
    shader_.set("uView", camera.view());
    shader_.set("uProjection", camera.projection(static_cast<float>(width)/static_cast<float>(height)));
    shader_.set("uEye", camera.position());
    shader_.set("uAmbient", scene.lighting.ambient);
    shader_.set("uDirDirection", scene.lighting.direction);
    shader_.set("uDirColor", scene.directionalEnabled ? scene.lighting.directionalColor : glm::vec3(0));
    shader_.set("uPointPosition", scene.lighting.pointPosition);
    shader_.set("uPointColor", scene.pointEnabled ? scene.lighting.pointColor : glm::vec3(0));
    shader_.set("uAttenuation", scene.lighting.attenuation);
    shader_.set("uTexture", 0);
    checker_.bind();
    for (const auto& object : scene.objects) {
        const glm::mat4 model = object.model(scene.angle);
        shader_.set("uModel", model);
        // Non-uniform scale does not preserve normals. Inverse transpose does.
        shader_.set("uNormalMatrix", glm::inverseTranspose(glm::mat3(model)));
        shader_.set("uAlbedo", object.material.albedo);
        shader_.set("uSpecular", scene.specularEnabled ? object.material.specular : 0.f);
        shader_.set("uShininess", object.material.shininess);
        shader_.set("uUseTexture", static_cast<int>(scene.textureEnabled && object.material.textured));
        shader_.set("uUvScale", object.material.uvScale);
        scene.meshes.at(object.mesh).draw();
    }
    glUseProgram(0);
}
void Renderer::capture(const std::string& path, int width, int height) const {
    // Read back before SwapBuffers. PPM stores top-to-bottom RGB; GL is bottom-up.
    std::vector<unsigned char> rgb(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*3);
    GLint pack = 4; glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    glPixelStorei(GL_PACK_ALIGNMENT, pack);
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot create capture: " + path);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    const auto stride = static_cast<std::size_t>(width)*3;
    for (int y = height-1; y >= 0; --y)
        file.write(reinterpret_cast<const char*>(rgb.data()+static_cast<std::size_t>(y)*stride),
                   static_cast<std::streamsize>(stride));
    if (!file) throw std::runtime_error("Capture write failed: " + path);
}
}
```

### `src/Scene.cpp`

```cpp
#include "pa3/Scene.h"
#include "pa3/Primitives.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
namespace pa3 {
glm::mat4 Object::model(float angle) const {
    // Column vectors: scale first, then rotate, then translate. Never use a zero scale.
    glm::mat4 m = glm::translate(glm::mat4(1.f), position);
    if (animated) m = glm::rotate(m, angle, glm::normalize(glm::vec3(.3f,1.f,.2f)));
    return glm::scale(m, scale);
}
Scene::Scene() {
    meshes.reserve(5);
    meshes.emplace_back(primitives::cube());
    meshes.emplace_back(primitives::sphere());
    meshes.emplace_back(primitives::cylinder());
    meshes.emplace_back(primitives::cylinder(true));
    meshes.emplace_back(primitives::torus());
    objects = {
        {"Ground",0,{0,-.15f,0},{11,.3f,8},{{.8f,.8f,.8f},.12f,24,true,3},false},
        {"Cube",0,{-3,1,1.3f},{1.6f,2,1.6f},{{.72f,.10f,.055f},.35f,40,false,1},false},
        {"Sphere",1,{0,1.05f,1.8f},{1.05f,1.05f,1.05f},{{.035f,.26f,.65f},.8f,96,false,1},false},
        {"Cylinder",2,{3,1,1.1f},{.8f,1,.8f},{{.055f,.48f,.20f},.45f,64,false,1},false},
        {"Cone",3,{-1.9f,1.35f,-2},{1,1.35f,1},{{.85f,.46f,.06f},.35f,32,false,1},false},
        {"Torus",4,{1.7f,1.6f,-1.8f},{1.2f,1.2f,1.2f},{{.44f,.07f,.60f},.75f,80,false,1},true}
    };
}
void Scene::reset(Camera& camera) {
    lighting = Lighting{};
    textureEnabled = specularEnabled = pointEnabled = directionalEnabled = animate = true;
    angle = 0.f;
    objects[2].material.albedo = {.035f,.26f,.65f};
    objects[2].material.shininess = 96.f;
    camera.preset(0);
}
void Scene::applyCase(int index, Camera& camera) {
    reset(camera);
    switch (index) {
    case 0: break;
    case 1: lighting.pointPosition = {4,2,-3}; break;
    case 2: objects[2].material.albedo = {.65f,.055f,.12f}; break;
    case 3: objects[2].material.shininess = 8.f; break;
    case 4: textureEnabled = false; break;
    case 5: camera.preset(1); break;
    default: throw std::runtime_error("Case must be in [0,5]");
    }
    std::cout << "Experiment " << index << ": " << status() << '\n';
}
void Scene::input(Window& window, Camera& camera, float dt) {
    for (int i = 0; i <= 5; ++i)
        if (window.pressed(GLFW_KEY_0+i)) applyCase(i, camera);
    if (window.pressed(GLFW_KEY_T)) textureEnabled = !textureEnabled;
    if (window.pressed(GLFW_KEY_B)) specularEnabled = !specularEnabled;
    if (window.pressed(GLFW_KEY_P)) pointEnabled = !pointEnabled;
    if (window.pressed(GLFW_KEY_L)) directionalEnabled = !directionalEnabled;
    if (window.pressed(GLFW_KEY_SPACE)) animate = !animate;
    if (window.pressed(GLFW_KEY_R)) reset(camera);
    const auto axis = [&window](int plus, int minus) {
        return static_cast<float>(window.down(plus))-static_cast<float>(window.down(minus));
    };
    lighting.pointPosition += 3.f*dt*glm::vec3(axis(GLFW_KEY_D,GLFW_KEY_A),axis(GLFW_KEY_E,GLFW_KEY_Q),axis(GLFW_KEY_K,GLFW_KEY_I));
    lighting.pointPosition = glm::clamp(lighting.pointPosition, glm::vec3(-10,.2f,-10), glm::vec3(10,10,10));
    auto& exponent = objects[2].material.shininess;
    if (window.pressed(GLFW_KEY_LEFT_BRACKET)) exponent = std::max(4.f, exponent*.5f);
    if (window.pressed(GLFW_KEY_RIGHT_BRACKET)) exponent = std::min(256.f, exponent*2.f);
    camera.update(window, dt);
}
void Scene::update(float dt) {
    if (animate) angle = std::fmod(angle + dt*.7f, glm::two_pi<float>());
}
std::string Scene::status() const {
    std::ostringstream text;
    text << "PA3 | tex " << textureEnabled << " | spec " << specularEnabled
         << " | dir " << directionalEnabled << " | point " << pointEnabled
         << " (" << lighting.pointPosition.x << ',' << lighting.pointPosition.y << ',' << lighting.pointPosition.z
         << ") | sphere exponent " << objects[2].material.shininess << " | anim " << animate;
    return text.str();
}
}
```

### `src/Shader.cpp`

```cpp
#include "pa3/Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <vector>
namespace pa3 {
namespace {
GlResource compile(GLenum stage, const char* source) {
    GlResource shader(GlResource::Kind::Shader, stage);
    const GLuint id = shader.get();
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    GLint ok = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint size = 0; glGetShaderiv(id, GL_INFO_LOG_LENGTH, &size);
        std::vector<char> log(static_cast<std::size_t>(std::max(size, 1)));
        glGetShaderInfoLog(id, size, nullptr, log.data());
        throw std::runtime_error(std::string(stage == GL_VERTEX_SHADER ? "Vertex: " : "Fragment: ") + log.data());
    }
    return shader;
}
}
Shader::Shader(const char* vertex, const char* fragment) {
    auto vs = compile(GL_VERTEX_SHADER, vertex);
    auto fs = compile(GL_FRAGMENT_SHADER, fragment);
    glAttachShader(program_.get(), vs.get());
    glAttachShader(program_.get(), fs.get());
    glLinkProgram(program_.get());
    GLint ok = GL_FALSE;
    glGetProgramiv(program_.get(), GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint size = 0; glGetProgramiv(program_.get(), GL_INFO_LOG_LENGTH, &size);
        std::vector<char> log(static_cast<std::size_t>(std::max(size, 1)));
        glGetProgramInfoLog(program_.get(), size, nullptr, log.data());
        throw std::runtime_error(std::string("Link: ") + log.data());
    }
    glDetachShader(program_.get(), vs.get());
    glDetachShader(program_.get(), fs.get());
}
GLint Shader::location(const std::string& name) {
    const auto found = locations_.find(name);
    if (found != locations_.end()) return found->second;
    const GLint value = glGetUniformLocation(program_.get(), name.c_str());
    if (value < 0) throw std::runtime_error("Missing active uniform: " + name);
    locations_.emplace(name, value);
    return value;
}
void Shader::set(const std::string& n, int v) { glUniform1i(location(n), v); }
void Shader::set(const std::string& n, float v) { glUniform1f(location(n), v); }
void Shader::set(const std::string& n, const glm::vec3& v) { glUniform3fv(location(n), 1, glm::value_ptr(v)); }
void Shader::set(const std::string& n, const glm::mat3& v) { glUniformMatrix3fv(location(n), 1, GL_FALSE, glm::value_ptr(v)); }
void Shader::set(const std::string& n, const glm::mat4& v) { glUniformMatrix4fv(location(n), 1, GL_FALSE, glm::value_ptr(v)); }
}
```

### `src/Texture.cpp`

```cpp
#include "pa3/Texture.h"
#include <array>
#include <cstdint>
namespace pa3 {
Texture::Texture() {
    constexpr int size = 256;
    std::array<std::uint8_t, size*size*3> pixels{};
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const bool light = ((x/32 + y/32) % 2) == 0;
        const std::array<std::uint8_t,3> rgb = light
            ? std::array<std::uint8_t,3>{192,202,216} : std::array<std::uint8_t,3>{72,85,105};
        for (int c = 0; c < 3; ++c) pixels[static_cast<std::size_t>((y*size+x)*3+c)] = rgb[static_cast<std::size_t>(c)];
    }
    bind();
    GLint previous = 4; glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Hardware decodes sRGB texels to linear values before lighting.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
}
```

### `src/Window.cpp`

```cpp
#include "pa3/Window.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace pa3 {
namespace {
void glfwError(int code, const char* text) {
    std::cerr << "GLFW " << code << ": " << text << '\n';
}
void GLAPIENTRY debugMessage(GLenum, GLenum type, GLuint id, GLenum severity,
                            GLsizei, const GLchar* message, const void*) {
    // Never throw through a C driver callback.
    std::cerr << "GL debug [" << id << "] type=" << type
              << " severity=" << severity << ": " << message << '\n';
}
}
GlfwSession::GlfwSession() {
    glfwSetErrorCallback(glfwError);
    if (!glfwInit()) throw std::runtime_error("glfwInit failed");
}
GlfwSession::~GlfwSession() { glfwTerminate(); }
Window::Window() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    handle_.reset(glfwCreateWindow(1280, 800, "PA3 | Lighting laboratory", nullptr, nullptr));
    if (!handle_) throw std::runtime_error("OpenGL 3.3 Core context unavailable");
    glfwMakeContextCurrent(native());
    glewExperimental = GL_TRUE;
    const GLenum result = glewInit();
    if (result != GLEW_OK)
        throw std::runtime_error(reinterpret_cast<const char*>(glewGetErrorString(result)));
    // Some GLEW versions probe legacy extensions in Core, setting INVALID_ENUM.
    while (glGetError() != GL_NO_ERROR) {}
    if (!GLEW_VERSION_3_3) throw std::runtime_error("OpenGL >= 3.3 required");
    if ((GLEW_VERSION_4_3 || GLEW_KHR_debug) && glDebugMessageCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugMessage, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        std::cout << "Debug callback enabled\n";
    } else {
        std::cout << "KHR_debug unavailable: using glGetError checks\n";
    }
    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    // Shader encodes linear output to sRGB; do not encode a second time.
    glDisable(GL_FRAMEBUFFER_SRGB);
    std::cout << "OpenGL: " << glGetString(GL_VERSION)
              << "\nRenderer: " << glGetString(GL_RENDERER) << '\n';
    GLint bits = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH,
        GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &bits);
    if (bits == 0) throw std::runtime_error("Default framebuffer has no depth buffer");
    std::cout << "Depth bits: " << bits << '\n';
    checkErrors();
}
bool Window::down(int key) const { return glfwGetKey(native(), key) == GLFW_PRESS; }
bool Window::pressed(int key) {
    const bool current = down(key);
    const bool edge = current && !previous_.at(static_cast<std::size_t>(key));
    previous_.at(static_cast<std::size_t>(key)) = current;
    return edge;
}
std::pair<int, int> Window::framebuffer() const {
    int width = 0, height = 0;
    glfwGetFramebufferSize(native(), &width, &height);
    return {width, height};
}
void Window::checkErrors() const {
    std::string errors;
    for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
        errors += " " + std::to_string(e);
    if (!errors.empty()) throw std::runtime_error("OpenGL errors:" + errors);
}
}
```

### `src/main.cpp`

```cpp
#include "pa3/Window.h"
#include "pa3/Renderer.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace {
int integer(const char* text) {
    std::size_t used = 0;
    const std::string value(text);
    const int result = std::stoi(value, &used);
    if (used != value.size()) throw std::runtime_error("Invalid integer: " + value);
    return result;
}
}
int main(int argc, char** argv) {
    try {
        int frameLimit = 0, experiment = 0;
        std::string capture;
        for (int i = 1; i < argc; ++i) {
            const std::string option(argv[i]);
            if (option == "--help") {
                std::cout << "PA3Scene [--frames N] [--case 0..5] [--capture image.ppm]\n";
                return 0;
            }
            if (i+1 >= argc) throw std::runtime_error("Missing value for " + option);
            if (option == "--frames") {
                frameLimit = integer(argv[++i]);
                if (frameLimit <= 0) throw std::runtime_error("Frame count must be positive");
            } else if (option == "--case") experiment = integer(argv[++i]);
            else if (option == "--capture") capture = argv[++i];
            else throw std::runtime_error("Unknown option: " + option);
        }
        if (experiment < 0 || experiment > 5) throw std::runtime_error("Case must be in [0,5]");
        if (!capture.empty() && frameLimit == 0) frameLimit = 1;
        pa3::Window window;
        {
            // All GPU-owning objects are destroyed while window/context is still alive.
            pa3::Renderer renderer;
            pa3::Scene scene;
            pa3::Camera camera;
            scene.applyCase(experiment, camera);
            window.checkErrors();
            std::cout << "Arrows: orbit | W/S: zoom | A/D Q/E I/K: move point light\n"
                      << "T: texture | B: specular | P/L: lights | Space: pause | R: reset\n"
                      << "0..5: experiments | [/]: sphere shininess | Esc: quit\n";
            double last = glfwGetTime(), nextTitle = 0;
            int rendered = 0;
            while (!glfwWindowShouldClose(window.native())) {
                glfwPollEvents();
                if (window.down(GLFW_KEY_ESCAPE)) break;
                const double now = glfwGetTime();
                const float dt = frameLimit > 0 ? 1.f/60.f : static_cast<float>(std::clamp(now-last, 0.0, .05));
                last = now;
                const auto [width, height] = window.framebuffer();
                if (width <= 0 || height <= 0) { glfwWaitEventsTimeout(.05); continue; }
                scene.input(window, camera, dt);
                scene.update(dt);
                renderer.draw(scene, camera, width, height);
                if (!capture.empty() && rendered+1 == frameLimit) renderer.capture(capture,width,height);
                window.checkErrors();
                glfwSwapBuffers(window.native());
                if (now >= nextTitle) {
                    const auto title = scene.status();
                    glfwSetWindowTitle(window.native(), title.c_str());
                    nextTitle = now+.2;
                }
                ++rendered;
                if (frameLimit > 0 && rendered >= frameLimit) break;
            }
        }
        window.checkErrors();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal: " << error.what() << '\n';
        return 1;
    }
}
```

### `shaders/scene.frag`

```glsl
#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;
out vec4 fragColor;
uniform vec3 uEye;
uniform vec3 uAmbient;
uniform vec3 uDirDirection;
uniform vec3 uDirColor;
uniform vec3 uPointPosition;
uniform vec3 uPointColor;
uniform vec3 uAttenuation;
uniform vec3 uAlbedo;
uniform float uSpecular;
uniform float uShininess;
uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform float uUvScale;
vec3 safeNormalize(vec3 value) {
    return value * inversesqrt(max(dot(value, value), 1e-12));
}
vec3 lightContribution(vec3 N, vec3 V, vec3 L, vec3 color, vec3 albedo) {
    float diffuse = max(dot(N,L), 0.0);
    vec3 H = safeNormalize(L+V);
    // Do not light the back of a surface with a specular highlight.
    float specular = diffuse > 0.0 ? pow(max(dot(N,H),0.0),uShininess) : 0.0;
    return color * (albedo*diffuse + vec3(uSpecular*specular));
}
vec3 linearToSrgb(vec3 linearColor) {
    vec3 c = clamp(linearColor, 0.0, 1.0);
    vec3 low = 12.92*c;
    vec3 high = 1.055*pow(c, vec3(1.0/2.4))-0.055;
    return mix(high, low, lessThanEqual(c, vec3(0.0031308)));
}
void main() {
    vec3 albedo = uAlbedo;
    if (uUseTexture) albedo *= texture(uTexture, vUv*uUvScale).rgb;
    vec3 N = safeNormalize(vNormal);
    vec3 V = safeNormalize(uEye-vWorldPosition);
    vec3 toPoint = uPointPosition-vWorldPosition;
    float d = length(toPoint);
    float attenuation = 1.0 / max(uAttenuation.x+uAttenuation.y*d+uAttenuation.z*d*d, 1e-6);
    vec3 linearColor = uAmbient*albedo;
    linearColor += lightContribution(N,V,safeNormalize(-uDirDirection),uDirColor,albedo);
    linearColor += attenuation * lightContribution(N,V,safeNormalize(toPoint),uPointColor,albedo);
    fragColor = vec4(linearToSrgb(linearColor),1.0);
}
```

### `shaders/scene.vert`

```glsl
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUv;
void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vNormal = uNormalMatrix * aNormal;
    vUv = aUv;
    gl_Position = uProjection * uView * world;
}
```

