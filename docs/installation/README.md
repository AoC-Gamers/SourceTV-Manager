# Instalación de SourceTV-Manager

El paquete contiene la extensión para Left 4 Dead 2 x86, su gamedata y
el include para plugins SourcePawn.

1. Instala Metamod:Source y SourceMod en el servidor.
2. Extrae el ZIP de la plataforma del servidor en `left4dead2/`.
3. Comprueba con `sm exts list` que SourceTV Manager carga.
4. Para compilar un plugin consumidor, usa `sourcetvmanager.inc` desde
   `addons/sourcemod/scripting/include/`.

Consulta el [repositorio de AoC](https://github.com/AoC-Gamers/SourceTV-Manager)
para consultar los artefactos y el contrato de la API.

## Actualización y reversión

Detén SRCDS y guarda los binarios y recursos SourceTV de la instalación actual.
Extrae el ZIP nuevo sobre el directorio del juego y repite la comprobación de
carga. Para revertir, detén el servidor, restaura esos archivos desde la copia y
retira los archivos añadidos por la actualización antes de volver a iniciar SRCDS.
