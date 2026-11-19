# Dependencias

[`scripts/deps/build-deps.lock`](../../scripts/deps/build-deps.lock) registra
los commits de SourceMod 1.12, Metamod:Source 1.12, HL2SDK L4D2 y AMBuild 2.2.
La herramienta local y el build en contenedor usan las mismas revisiones.
La extensión incorpora SafetyHook y sus dependencias Zydis y Zycore mediante enlace estático. La
revisión de
SafetyHook se fija mediante el submódulo `safetyhook/`.

La interfaz pública SourcePawn está en
[`sourcetvmanager.inc`](../../addons/sourcemod/scripting/include/sourcetvmanager.inc).
Consulta los [requisitos de instalación](../installation/README.md) para el servidor y el gamedata.

El build no requiere Steamworks SDK.

## Verificación del build y del paquete

Los SDK de Git y AMBuild deben ser checkouts completos de las revisiones del
lock, con submódulos inicializados y el árbol limpio, incluidos archivos
ignorados. Un directorio de fuentes sin metadatos Git no acredita esas
revisiones. El build ejecuta AMBuild desde su checkout fijado con el intérprete
de `VENV_DIR`. Si usas rutas alternativas (`SOURCEMOD_DIR`, `MMSOURCE_DIR`,
`HL2SDK_DIR` o `AMBUILD_DIR`), pásalas también a los targets de empaquetado
y validación. Los recibos antiguos deben regenerarse con un nuevo build.

El checkout del proyecto debe conservar su gitlink de SafetyHook. En el
contenedor esa revisión se valida en el árbol montado del proyecto.

## Uso y distribución

| Dependencia | Uso | Distribución |
| --- | --- | --- |
| SourceMod 1.12 y Metamod:Source 1.12 | SDK de compilación y prerrequisitos del servidor | Instalaciones externas |
| HL2SDK del juego | Headers y bibliotecas de enlace según la plataforma | El SDK se obtiene mediante el lock y queda fuera del ZIP |
| AMBuild y Python | Configuración, compilación y empaquetado | Herramientas de desarrollo |
| SafetyHook, Zydis y Zycore | Código incorporado por enlace estático | Avisos en [`NOTICE.md`](../../NOTICE.md) |
| SourcePawn 1.12 | Compilación opcional del diagnóstico | Compilador y diagnóstico fuera del ZIP |

Los commits concretos están en el lock o en los submódulos asociados. Los
resultados de compilación describen la matriz probada del toolchain. La
compatibilidad mínima de carga se comprueba por juego y versión en SRCDS.
