# Changelog

## [Unreleased]

### Cambiado

- **Distribución.** ZIP Linux y Windows para L4D2 x86 con la extensión, el gamedata
  y el include público. Se retiran los paquetes para otros juegos y las variantes
  compiladas con SourceMod 1.10 y 1.11. Si dependes de una de esas variantes,
  conserva el paquete anterior que usa tu servidor. El ZIP incluye el README
  del proyecto y se verifica contra los binarios y dependencias de la compilación.

### Corregido

- La grabación gestionada queda indisponible si falla un hook de inicio o
  cierre. Se retiran los hooks instalados parcialmente.
- La impresión a espectadores valida el receptor y restaura el idioma previo
  del relay y el destino de traducción después de formatear.

- **Grabación.** Una operación anidada que reemplaza la sesión invalida las
  acciones pendientes sobre la sesión anterior, incluso con el mismo nombre
  de archivo. Una parada interna sin archivo abierto conserva el inicio y
  su token de propietario. Los cierres preservan el archivo y tick previos.

- **Espectadores.** Las conexiones y el chat anidados tienen contextos
  independientes. Los rechazos vacíos se inicializan y los motivos con `%`
  se muestran completos en L4D2. La conexión conserva el orden de hooks de SourceHook y los comandos
  de SourceTV restauran el host y el canal previos.
- **Ciclo de vida.** Los forwards de inicio observan una instancia registrada.
  Al cerrar, la extensión retira los hooks de los receptores que registró.
  Libera las llamadas de eventos locales antes de que se retire BinTools.

- **Eventos locales y chat.** En L4D2 Linux x86, ambas rutas usan el
  desplazamiento del subobjeto que devuelve el motor. El envío local de eventos
  y los forwards de chat usan la misma entrada de gamedata.

### Desarrollo

- CI ejecuta las regresiones nativas Linux x86 con ASan y UBSan y prueba
  las fronteras que usan el SDK en el job de compilación.
- El recibo de cobertura local detecta cambios en headers, dependencias,
  órdenes, toolchain y pruebas antes de importar los informes.

- Cada push a `develop` publica la prerelease con los ZIP validados de Linux y Windows.

- GNU Make centraliza la preparación de dependencias, la compilación, las pruebas y el empaquetado local y en CI.
