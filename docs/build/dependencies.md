# Dependencias

[`scripts/deps/build-deps.lock`](../../scripts/deps/build-deps.lock) registra
los commits de SourceMod 1.12, Metamod:Source 1.12, HL2SDK L4D2 y AMBuild 2.2.
La herramienta local y el build en contenedor usan las mismas revisiones.
La aplicación incluye SafetyHook y Zydis por enlace estático. La revisión de
SafetyHook se fija mediante el submódulo `safetyhook/`.

La interfaz pública SourcePawn está en
[`sourcetvmanager.inc`](../../addons/sourcemod/scripting/include/sourcetvmanager.inc).
El servidor necesita SourceMod y Metamod:Source. El gamedata acompaña al binario
y debe corresponder a la versión del juego.

## Uso y distribución

| Dependencia | Uso | Distribución |
| --- | --- | --- |
| SourceMod 1.12 y Metamod:Source 1.12 | SDK de compilación y prerrequisitos del servidor | Instalaciones externas |
| HL2SDK del juego | Headers y bibliotecas de enlace según la plataforma | El SDK se obtiene mediante el lock y queda fuera del ZIP |
| AMBuild y Python | Configuración, compilación y empaquetado | Herramientas de desarrollo |
| SafetyHook, Zydis y Zycore | Código incorporado por enlace estático | Avisos en `NOTICE.md` |
| SourcePawn 1.12 | Compilación opcional del diagnóstico | Compilador y diagnóstico fuera del ZIP |

Los commits concretos están en el lock o en los submódulos asociados. Los
resultados de compilación describen la matriz probada del toolchain. La
compatibilidad mínima de carga se comprueba por juego y versión en SRCDS.
