# Instalación de SourceTV-Manager

Usa un servidor dedicado de Left 4 Dead 2 x86. SourceMod 1.12 y Metamod:Source 1.12 son la
referencia de compilación. Conserva BinTools y SDKTools de SourceMod y usa el gamedata del mismo
paquete que el binario.

## Compatibilidad en Linux

El ZIP Linux de Develop inspeccionado se compiló con Ubuntu 26.04. La extensión de ese ZIP
requiere los símbolos `GLIBC_2.38` y `GLIBCXX_3.4.30` en las bibliotecas x86 del servidor.
Verifica la carga en SRCDS con las bibliotecas de tu distribución.
[Ejecución del build inspeccionado](https://github.com/AoC-Gamers/SourceTV-Manager/actions/runs/44411470131).

## Instalar y verificar

1. Descarga `sourcetvmanager-develop-linux.zip` o `sourcetvmanager-develop-windows.zip`, según
   el servidor.
   [Release Develop](https://github.com/AoC-Gamers/SourceTV-Manager/releases/tag/channel/develop).
2. Detén SRCDS y copia el contenido de `addons/` del ZIP a `left4dead2/addons/` sin renombrar
   archivos.
3. Inicia SRCDS. SourceMod puede cargar Manager al cargar un plugin que lo requiera. Para
   cargarlo manualmente, ejecuta `sm exts load sourcetvmanager.ext.2.l4d2` en la consola del
   servidor.
4. Ejecuta `sm exts list` y confirma que **SourceTV Manager** figure cargado. Con SourceTV
   activo, verifica la operación que usará tu plugin, por ejemplo el envío de un mensaje a un
   espectador o el inicio y cierre de una demo.

El ZIP incluye el binario en `addons/sourcemod/extensions/`, `sourcetvmanager.games.txt` en
`gamedata/` y `sourcetvmanager.inc` en `scripting/include/`. Usa ese include para compilar los
plugins consumidores. Consulta la [API SourcePawn](../api/README.md).

## Actualizar o revertir

Detén SRCDS y guarda una copia de los archivos que reemplazarás. Instala juntos el binario y los
recursos del ZIP nuevo y repite las comprobaciones de carga y funcionamiento. Para revertir,
detén SRCDS, restaura el conjunto anterior y retira únicamente los archivos añadidos por la
actualización.

Para compilar desde fuentes, consulta la [guía de compilación](../build/README.md).
