# API SourcePawn

Incluye [`sourcetvmanager.inc`](../../addons/sourcemod/scripting/include/sourcetvmanager.inc)
para acceder a la biblioteca `sourcetvmanager`. El include contiene las firmas, enumeraciones y
contratos de cada native y forward.

- Consulta la instancia SourceTV seleccionada, su estado, puerto, estadísticas y retardo.
- Accede a los espectadores, envía mensajes de chat o consola y recibe notificaciones de
  conexión y chat.
- Selecciona planos de cámara mediante el director de SourceTV.
- Inicia y detén demos, consulta el tick de grabación y escribe mensajes en la consola de la demo.

## Grabación y propiedad

Llama a `SourceTV_StartRecordingEx(filename, continuous, ownerToken)` para iniciar una demo en
la instancia seleccionada y vincularla al plugin que llama. Un resultado
`SourceTVRecordingStart_Started` indica que el motor confirmó la grabación y el nombre efectivo.
El token devuelto es positivo y se asigna una sola vez durante la vida de la extensión.

Los demás resultados distinguen grabador no disponible, SourceTV inactivo, instancia que no es
el proxy principal, grabación en curso, ruta inválida, fallo de inicio y falta de un token
disponible. `continuous` se pasa al motor. En Left 4 Dead 2 la rotación de archivos y la
continuidad entre mapas requieren validación propia.

- `SourceTV_StopOwnedRecording(ownerToken)` detiene únicamente la grabación activa del plugin
  llamante con ese token.
- `SourceTV_StopRecording()` también admite demos sin propietario y rechaza detener una demo
  administrada por otro plugin. Los comandos directos del motor quedan fuera de este control.
- Al descargarse el plugin propietario, se revoca su token y la demo sigue activa como grabación
  sin propietario. Un consumidor puede consultar su estado antes de decidir si la detiene.

## Estado y notificaciones

`SourceTV_GetRecordingSnapshot(...)` consulta sincrónicamente el grabador de la instancia
seleccionada. Devuelve si está disponible y entrega el estado de grabación, token, instancia,
ruta y tick. Solo el propietario recibe su token. Cuando el grabador no está disponible, las
salidas son `false`, `0`, `-1`, ruta vacía y tick `-1`, respectivamente.

| Forward | Cuándo se emite |
| --- | --- |
| `SourceTV_OnRecorderAvailabilityChanged` | Aparece o desaparece un grabador. Los plugins que se cargan tarde consultan el snapshot para conocer el estado vigente. |
| `SourceTV_OnRecordingFileStarted` | El motor retorna con una grabación activa. Incluye el argumento `continuous` recibido por el motor. |
| `SourceTV_OnStopRecording` | Antes de que el motor cierre la demo. |
| `SourceTV_OnRecordingFileClosed` | Tras retornar del cierre, con el nombre y tick capturados antes de cerrar el archivo. Si el mismo archivo sigue activo, el aviso se omite. |

La propiedad termina al quedar inactivo el grabador. Una grabación posterior no recibe el token
anterior, incluso si se inicia tras un cambio de mapa. Una caída del proceso puede impedir la
notificación de cierre.

## Comportamiento del servidor

- `tv_force_steamauth` viene desactivada (`0`). Al activarla, Manager valida por Steam a los
  clientes SourceTV y rechaza las conexiones de relays.
- Un plugin puede registrar la salida de `status` en una demo mediante
  `FakeClientCommand(SourceTV_GetBotIndex(), "status")`. Esto requiere el bot del servidor
  SourceTV principal.

Consulta los [escenarios de validación en SRCDS](../testing/README.md).
