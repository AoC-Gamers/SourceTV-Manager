# Changelog

## [Unreleased]

### Cambiado

- **Distribución.** ZIP Linux y Windows para L4D2 x86 con la extensión, el gamedata
  y el include público. Se retiran los paquetes para otros juegos y las variantes
  compiladas con SourceMod 1.10 y 1.11. Si dependes de una de esas variantes,
  conserva el paquete anterior que usa tu servidor. El ZIP incluye el README
  del proyecto y se verifica contra los binarios y dependencias de la compilación.

### Desarrollo

- GNU Make centraliza la preparación de dependencias, la compilación, las pruebas y el empaquetado local y en CI.
