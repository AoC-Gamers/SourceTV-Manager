# SourceTV-Manager

Extensión de SourceMod que permite a los plugins SourcePawn consultar y controlar SourceTV en servidores dedicados de Left 4 Dead 2.

Gracias a Peace-Maker y a quienes contribuyeron al [proyecto original `peace-maker/sourcetvmanager`](https://github.com/peace-maker/sourcetvmanager). El trabajo de AoC-Gamers se centra en la calidad del código, las pruebas y la distribución de paquetes instalables para Linux y Windows.

> **Linux x86.** Los ZIP Develop de AoC verificados se compilaron con Ubuntu 26.04. Comprueba las bibliotecas de 32 bits del servidor antes de instalar el ZIP. Consulta los [requisitos del servidor](docs/installation/README.md#compatibilidad-en-linux).

## Instalación rápida

1. Usa un servidor dedicado de Left 4 Dead 2 de 32 bits con Metamod:Source y SourceMod. Las versiones de referencia son 1.12 de ambos.
2. Descarga el ZIP de Linux o Windows desde [Releases](https://github.com/AoC-Gamers/SourceTV-Manager/releases).
3. Con el servidor detenido, copia el árbol `addons/` del ZIP a `left4dead2/`, conservando las rutas.
4. Inicia el servidor y verifica la carga con `sm exts list`. Si Manager no aparece, ejecuta `sm exts load sourcetvmanager.ext.2.l4d2` en la consola del servidor.

## Documentación

- [Guía de instalación](docs/installation/README.md) con verificación, actualización y reversión.
- [API SourcePawn](docs/api/README.md).
- [Índice de documentación](docs/README.md) con compilación, dependencias y pruebas.

Consulta la [licencia](LICENSE), las [atribuciones](NOTICE.md) y el [historial de cambios](CHANGELOG.md).
