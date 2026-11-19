# Validación de SourceTV-Manager en SRCDS

## Antes de probar

Usa un SRCDS de pruebas con las [dependencias de instalación](../installation/README.md).
Registra SO, versión y build de L4D2, versiones de Metamod:Source y SourceMod, revisión
disponible y SHA-256 de los ZIP y binarios cargados. Conserva logs, transcripciones y demos
fuera del instalador.

Para revisar una demo, copia el archivo `.dem` del servidor al directorio `left4dead2/` del
cliente y ejecuta `playdemo <nombre>` en su consola. Verifica la reproducción hasta el cierre.

## Escenarios

| Escenario | Resultado esperado |
| --- | --- |
| Carga | `sm exts list` muestra **SourceTV Manager**. Un plugin consumidor puede llamar a la API con SourceTV activo. |
| Grabación | El inicio devuelve un token y el snapshot refleja la instancia y la ruta. Prueba un token ajeno y uno revocado. Cierra con el propietario y verifica los forwards y la reproducción. |
| Ciclo de vida | Descarga el plugin propietario durante una grabación. Comprueba que el token pierde validez, que el snapshot presenta la demo sin propietario y que la demo puede cerrarse usando la API correspondiente. Cambia de mapa y comprueba la nueva instancia. |
| Espectadores | Conecta un cliente al puerto SourceTV, envíale un mensaje de chat y otro de consola, y verifica ambos en el cliente. Prueba con la traducción en español latinoamericano y comprueba los rechazos con motivo vacío y con un `%` literal. |
| Convivencia | Carga Manager y Support en ambos órdenes. Descarga y recarga cada extensión mientras la otra sigue activa. Reconecta al espectador y repite los mensajes y la grabación. |
| Dependencias | En una instancia de pruebas, retira y restablece BinTools y SDKTools. Comprueba el estado de las funciones que dependen de cada proveedor y que no se utilicen después de su retirada. |

En Linux, `make test-coexistence` comprueba con SourceHook ambos órdenes de carga y retirada, el
rechazo y las conexiones anidadas con un proveedor controlado. Consulta las
[pruebas automatizadas](../../tests/README.md) para distinguir ese alcance de la validación en
SRCDS.
