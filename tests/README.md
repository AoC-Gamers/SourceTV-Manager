# Pruebas de SourceTV-Manager

`make test-ci` comprueba procedencia de dependencias, esquema y contenido
del paquete, y el rechazo de entradas inválidas antes de publicar. Usa checkouts
temporales y dobles del publicador, sin descargas ni Sonar.

`make test-public-api` compara el contrato con la fixture congelada y también
se ejecuta desde `make test-ci`. Consulta el [baseline y la evidencia
ABI](native-baseline.md).

`make test-abi` usa el JIT x86 de BinTools del checkout fijado. El proveedor
controlado comprueba el receptor, el puntero de evento, el bool y el número
de llamadas. Detecta el receptor incorrecto que resulta del desplazamiento
anterior en un objeto de prueba. Requiere los SDK locales y multilib de Linux.

`make test-native` compila y ejecuta la aceptación de rutas de grabación
sobre código de producción. En Linux usa un compilador C++17 con multilib
x86 por defecto. Se puede seleccionar el compilador mediante `CXX` y los flags
mediante `NATIVE_CXXFLAGS`. Cada ejecución crea su propio directorio en
`.build/` y guarda el compilador, flags y resultado. La suite pura se ejecuta en CI
mediante `make test-native-sanitized` con ASan y UBSan. Funciona sin SDK y
los proveedores controlados se prueban por separado del motor.

Para la frontera Pawn usa `make test-pawn-boundary`. Compila con los headers
de SourcePawn del checkout `SOURCEMOD_DIR` y usa un contexto controlado.
Para comprobar también la VM local, usa
`make test-pawn-boundary-vm SPCOMP=/ruta/a/spcomp PAWN_VM=/ruta/a/libsourcepawn.so`.
El compilador C++ (flags `PAWN_VM_CXXFLAGS`) y la biblioteca de la VM deben
tener la misma arquitectura. Usa una biblioteca y un `spcomp` del commit
fijado de SourcePawn. Ninguna de estas pruebas descarga dependencias.

| Natives | Resultado ante una entrada inválida |
|---|---|
| `GetLocalStats`, `GetGlobalStats` | Falso y salidas intactas si una celda es inválida |
| `GetViewOrigin` | Devuelve 0 y deja el vector intacto si sus tres celdas no son accesibles |
| `GetServerIP`, `GetDemoFileName` | Falso ante capacidad no positiva, rango inválido o fallo de escritura |
| `GetClientName`, `GetClientIP`, `GetClientPassword` | Retorno void; un rango inválido deja el buffer intacto |
| `GetRecordingSnapshot` | Falso sin escribir si falla el rango de una salida. Una capacidad cero omite el nombre. Los defaults del grabador ausente se mantienen con salidas válidas. |
| `StartRecording`, `StartRecordingEx`, `KickClient` | Una dirección inválida o un string sin NUL accesible aborta la operación. En el inicio extendido devuelve `InvalidPath` y deja el token intacto si el rango de entrada o salida es inválido. |
| `BroadcastScreenMessage`, `BroadcastChatMessage` | Falso para valores ajenos a `SourceTVBroadcastTarget` |
| Natives con formato | Aborta antes de formatear si la dirección del formato es inválida o el texto carece de NUL accesible. SourceMod sigue interpretando los argumentos variables. |
| `ForceFixedCameraShot`, `ForceChaseCameraShot` | Falso ante rangos Pawn requeridos inválidos, valores no representables o tick fuera de rango durante la validación inicial |

La frontera comprueba bytes accesibles en la memoria de la VM. El API público
no revela el tamaño de la declaración original de un array Pawn. La capacidad
sigue siendo parte del contrato del caller. La VM local cubre el límite de heap,
canarios y truncamiento UTF-8. La integración de natives completos con SourceMod
y las llamadas de cámara/grabación en SRCDS requieren pruebas separadas. Las
cámaras conservan el orden existente de acciones ante un fallo de `CreateEvent`.

`make test-native` también ejecuta la lógica de sesiones de grabación con un
grabador controlado. Comprueba precondiciones, confirmación de archivo, rutas,
propiedad por plugin y serial, agotamiento de tokens, snapshots de cierre,
reentrada y reutilización de direcciones. El estado comprueba el contexto de
cada operación antes de sus efectos. Si una parada anidada cierra o sustituye
la sesión durante el forward legacy, la parada exterior omite la llamada al
motor. El forward de cierre conserva el archivo y tick capturados antes de
la parada. Los dobles prueban esas decisiones en C++, sin ejecutar callbacks
SourceHook ni el motor de L4D2. La continuidad entre mapas sigue pendiente
de prueba funcional.

La instalación de los observadores de grabación se prueba con fallos de
creación y habilitación en Linux y de registro en Windows. El grabador solo
se anuncia como disponible para las operaciones gestionadas cuando puede
observar inicio y cierre. Ante un fallo, se retiran los registros parciales y
`StartRecordingEx` devuelve `RecorderUnavailable` con token cero. Las
consultas directas de solo lectura siguen consultando el grabador del motor.

El adaptador de impresión usa dobles para probar slots fuera de rango, relay
ausente, traducción anidada, fallo de formato y reutilización del receptor.
Comprueba que se restaure el idioma previo del relay y el destino global.
La impresión de espectadores conectados se verifica en SRCDS con un cliente.

`make test-native` prueba además buffers independientes de conexión, chat
anidado por instancia, y restauración del host y del canal al finalizar o
retirar al propietario. El wrapper conserva la contraseña efectiva que se
pasa al motor. En SRCDS queda por comprobar el rechazo vacío y con `%`, la
consulta de la instancia dentro de `SourceTV_OnServerStart` y la retirada de
hooks con Manager y Support en ambos órdenes de carga.

Las natives conservan una única [tabla de registro](../extension/natives/natives.cpp).
Sus operaciones de [grabación](../extension/natives/recording.cpp),
[cámara](../extension/natives/camera.cpp) y
[salida](../extension/natives/output.cpp) se compilan por separado.
Los [observadores de grabación](../extension/hooks/recording.cpp) reúnen
la instalación, los callbacks y la retirada para cada plataforma.

Para el diagnóstico de grabación, ejecuta
`make build-diagnostics SPCOMP=/ruta/a/spcomp SPCOMP_INCLUDE_DIR=/ruta/a/include`
con un toolchain SourceMod 1.12. El resultado queda en
`.build/diagnostics/sourcetv_test.smx`. Usa el diagnóstico en una instancia
de pruebas con acceso controlado y la extensión del ZIP correspondiente.

1. Con SourceTV activo, ejecuta `stv_test_snapshot` y registra los campos
   devueltos. Inicia una grabación con `stv_test_start_owned muestra` y
   comprueba el token positivo, el archivo, el forward de inicio y el snapshot.
2. Prueba un token incorrecto con `stv_test_stop_owned <token>`; la grabación
   debe seguir activa. Repite con el token correcto y confirma el forward de
   cierre, el tick final y que el snapshot vuelve a estado inactivo.
3. Inicia otra demo, guarda su token y descarga y recarga el diagnóstico.
   La demo debe seguir grabando como huérfana, con token cero. Comprueba que el
   token anterior no detiene la grabación. Detén la demo con el comando
   legacy del diagnóstico `sm_stoprecording`.
4. Cambia de mapa con una demo activa. Registra disponibilidad y cierre,
   comprueba que el token previo no controla la grabación del nuevo mapa y
   verifica los archivos de demo que produjo el motor.
