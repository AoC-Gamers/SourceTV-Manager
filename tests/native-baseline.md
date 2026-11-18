# Baseline de API y evidencia ABI

## Fuentes y contrato público

Baseline de `develop`: `174ec56460688453eb678290c98fc27f4bc2c334`, el mismo
SHA fijado en el informe. SteamWorks `1cd402268d8a0ba6f98c3a5463747fc2390d49fb`
sirve de referencia para pruebas y fronteras tipadas.

[La fixture congelada](fixtures/public-contract.json) fue tomada del árbol Git
anterior a la extracción. Registra 40 natives y sus marcas de opcionalidad,
14 forwards y sus registros, dos enums, el valor por defecto de cámara, el
nombre lógico de la extensión, la biblioteca y la cvar propia. El control
compara declaraciones y registros por separado para detectar faltantes y
duplicados. Conservar la fixture al mover código y adaptar el lector si cambia
la ubicación de los registros.

Con direcciones Pawn válidas, `StartRecordingEx` inicializa el token a cero y
solo lo entrega positivo tras confirmar inicio. El snapshot sin grabador
devuelve falso y escribe falso, 0, -1, nombre vacío si hay capacidad positiva,
y tick -1. Con grabador inactivo devuelve verdadero, indica la instancia y
conserva los demás defaults. Durante grabación solo el propietario recibe su
token. `StopOwnedRecording` informa si el grabador realmente quedó inactivo.
El stop legacy respeta el propietario y puede detener una demo sin dueño.
`GetDemoFileName` devuelve falso si no hay demo activa. `GetRecordingTick`
devuelve -1 cuando no hay grabación. Los fallos de traducción de salidas Pawn
requieren una caracterización con VM antes de modificar su tratamiento.

| Unidad | Responsabilidad y contratos sensibles |
|---|---|
| [Entrypoint](../extension/extension.cpp) | Biblioteca, registro de natives, adquisición de interfaces y ciclo de vida |
| [Natives](../extension/natives/natives.cpp) | Traducción Pawn, grabación, cámaras, espectadores y llamada local de eventos |
| [Forwards](../extension/hooks/forwards.cpp) y [hooks comunes](../extension/hooks/commonhooks.cpp) | Callbacks, conexión, contextos temporales, tokens y orden de grabación |
| [Servidor](../extension/core/hltvserverwrapper.cpp), [director](../extension/core/hltvdirectorwrapper.cpp) y [cliente](../extension/core/hltvclientwrapper.cpp) | Interfaces del motor, grabador, cámara y canal |
| [AMBuilder](../extension/AMBuilder), [SDK locales](../extension/sdk/engine/ihltvdemorecorder.h) y [gamedata](../addons/sourcemod/gamedata/sourcetvmanager.games.txt) | Selección de unidades, declaraciones y resolución por juego y plataforma |

El [mapa de paquete](../scripts/package/package-map.json) sigue siendo el
inventario instalable. El [lock](../scripts/deps/build-deps.lock) fija HL2SDK,
SourceMod, Metamod y AMBuild. SafetyHook conserva el gitlink
`ab76f94ee4ef28187cc08030599577996fcba3be`. Los headers públicos del HL2SDK
fijado, `public/ihltv.h` y `public/iserver.h`, distinguen `IHLTVServer` e
`IServer`. `GetBaseServer()` devuelve `IServer*`.

## Evidencia del motor por plataforma

Se inspeccionó estáticamente una copia local de `engine_srv.so` procedente
de una captura de SteamWorks. `readelf` confirmó ELF32 i386 y build ID
`b152439170af081a0d31fc357364a4cd4caa56ac`. SHA-256
`8f8093a18bba14cb73fc11589687bfb8156b00328b7413d697f1a76ed4b7621a`.
La versión del juego que corresponde al despliegue actual queda por cotejar.
Para Windows queda pendiente obtener `engine.dll` PE x86 identificada con
SHA-256 y versión del juego. La columna Windows describe evidencia de fuente.

| Contrato | Linux x86 | Windows x86 | Verificación pendiente |
|---|---|---|---|
| `CHLTVServer::ConnectClient` | `nm -C` muestra el método y un thunk. El símbolo usa `netadr_s&` y vector de objetos `CLC_SplitPlayerConnect`. El hook declara dirección const y vector de punteros. | Existe declaración y selector de gamedata. Módulo sin identificar. | Identificar vtable, objeto receptor, elementos del vector, convenio y retorno. Correlacionar en cada binario y probar SourceHook con proveedor independiente. |
| `GetBaseServer` y conversiones del servidor | El símbolo y su thunk existen. El SDK devuelve `IServer*`. La llamada local de eventos aplica un ajuste literal y el forward usa otra ruta con fallback. | SDK y ramas de fuente disponibles. Módulo sin identificar. | Trazar el subobjeto de cada ruta, el ajuste efectivo y el receptor de BinTools/SourceHook. Confirmar la llamada original. |
| `IDemoRecorder`, `CHLTVDemoRecorder` y `CDemoFile` | Los símbolos confirman nombres y parámetros de inicio/parada. El wrapper interpreta el comienzo del archivo como nombre. | Declaraciones locales y gamedata disponibles. Módulo sin identificar. | Confirmar slots, orden de métodos, campo filename y duración de préstamo del archivo. Probar datos tomados antes del cierre. |
| `IClient`, `CBaseClient`, `CGameClient` y canal temporal `CNetChan` | Contratos observados en el SDK, wrappers y hooks. | Contratos observados en el SDK, wrappers y hooks. | Confirmar bases, receptor virtual, duración de la sustitución y restauración ante reentrada. |
| `CHLTVDirector` y cámara | Los consumidores y el gamedata describen los campos usados. | Los consumidores y el gamedata describen los campos usados. | Contrastar ancho, offsets y ticks/origen en el módulo identificado antes de cualquier cambio. |

La evidencia de símbolos confirma lo que el símbolo codifica. Quedan abiertas
las llamadas reales, los layouts de objeto, los slots y el comportamiento
en SRCDS. Los perfiles se verificarán por separado con módulos identificados,
proveedores independientes y pruebas de marshalling, base/slot erróneos,
resolución incompleta y ciclo de vida. En esta fase no se modifican firmas,
gamedata ni adaptadores ABI.

## Comprobaciones locales

| Nivel | Resultado |
|---|---|
| `make test-ci` | 14 pruebas de paquete y una comparación de API |
| `make test-native` | 9 casos de la política de rutas en GCC 13.3.0, C++17 x86. Una mutación aislada que acepta la ruta vacía hizo fallar el caso correspondiente. |
| Build y paquete Linux | `build-exts-linux`, `package-exts-linux`, `release-linux` y `validate-artifact-linux` pasaron sobre la extracción. |
| Windows, SourceHook/BinTools, VM y SRCDS | Pendientes para la extracción. |


## Binding de eventos locales y chat

En el ELF32 identificado arriba, `CHLTVServer::GetBaseServer()` devuelve
`this + 4`. El thunk desde `IHLTVServer` ajusta primero `this` en
`-0x204`, por lo que ese puntero y el `IServer*` de retorno representan
subobjetos diferentes. Las entradas directas de `BroadcastLocalChat` y
`BroadcastEventLocal` reciben el objeto completo como primer argumento.

| Plataforma | Entrada de gamedata | Uso y evidencia |
|---|---|---|
| L4D2 Linux x86 | `IServer_from_CHLTVServer = 4` | Del objeto del detour a `IServer*` para el registro de chat. En sentido inverso, de `IServer*` al receptor de la llamada directa de eventos. Correlación estática con el ELF y llamada controlada mediante el BinTools fijado. |
| L4D2 Windows x86 | `IServer_from_CHLTVServer = 8` | Valor heredado de ambas rutas, ahora explícito. Queda pendiente cotejarlo con el PE del motor y ejecutar el probe y el escenario funcional en Windows. |

Si el desplazamiento de L4D2 falta o es negativo, el envío local de eventos
devuelve falso y el detour de chat queda desactivado. La llamada de eventos
conserva la convención y el orden del buffer de BinTools. En SRCDS queda
pendiente comprobar un mensaje local, un chat de espectador y el original
cuando el binding carece de configuración. Los contratos de `ConnectClient`
y del grabador requieren expedientes propios.
