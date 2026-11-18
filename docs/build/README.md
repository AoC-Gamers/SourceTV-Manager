# Compilación y distribución

Usa GNU Make, Git y Python 3. En Linux necesitas GCC/G++ con multilib x86.
En Windows necesitas Visual Studio 2022 con MSVC x86 y Bash de Git para Windows.
Consulta las [dependencias fijadas](dependencies.md).

```sh
make deps-exts-linux
make build-exts-linux
make package-exts-linux
make release-linux
make validate-artifact-linux
make test-ci
```

En Windows usa los targets con sufijo `-windows` y `PYTHON=python` si tu
instalación expone Python con ese nombre. Cada fase tiene su propio target.
Ejecuta las fases en el orden indicado y repite el build si cambian las fuentes.
Make usa `.deps/exts-<plataforma>/` para dependencias y `.build/` para AMBuild.
El staging queda en `dist/<plataforma>/artifact/` y el ZIP en `dist/release/`.
`RELEASE_CHANNEL` acepta `develop` (por defecto) y `latest`.

## CI

El job de build Linux usa la variable de organización `L4D2_EXT_LINUX_RUNNER`.
AoC la configura en `ubuntu-26.04` y la habilita para este repositorio.
Si queda vacía o el repositorio pierde acceso a la variable, la validación
previa falla con un mensaje antes de asignar el runner de compilación.

GitHub Actions ejecuta las pruebas del contrato y compila en Linux y Windows.
El recibo de compilación vincula las fuentes, las dependencias efectivas y
los binarios mediante SHA-256. El ZIP incluye el README del proyecto e
identifica la revisión o las fuentes locales usadas. Los enlaces a la guía
de compilación apuntan al commit base cuando el build contiene cambios locales.
Los ZIP de runs de `develop` usan el canal Develop. Los runs de `master` o
`main` usan Latest. Actions conserva los ZIP como artefactos verificables
del run. El target `release-*` genera el archivo local. Los tags `v*` generan
ZIP versionados. Los PR y otras ramas usan el SHA corto como identificador.

La ejecución manual con `publish_release` publica ambos ZIP del mismo run,
una vez validados Linux y Windows. `develop` actualiza `channel/develop` como
prerelease. `master` y `main` actualizan `channel/latest`. Las actualizaciones
de canal pueden dejar una pausa entre la retirada de la release anterior y
la publicación de la nueva. Un fallo de subida se reintenta desde Actions.

## Sonar local

Tras un build Linux en el host, `make sonar-cppcheck` usa las órdenes reales de compilación
L4D2 x86 y escribe el informe en `.build/sonar/`. Instala Cppcheck con soporte
para bases de compilación. `make sonar` añade SonarScanner y requiere un servidor
SonarQube con sonar-cxx, `SONAR_HOST_URL` y `SONAR_TOKEN`. Usa los archivos de
ejemplo para configurar el análisis. El informe refleja análisis estático.
`make test-package-coverage` mide las pruebas del empaquetador con coverage.py
y genera el informe local en `.build/package-coverage/`. El análisis en el
servidor SonarQube y la cobertura nativa requieren sus propias ejecuciones. CI ejecuta las pruebas del producto sin llamar a Sonar.

## Contenedor

`make build-linux-container` prepara SafetyHook e invoca Compose con un
toolchain Linux x86 fijado. El contenedor compila y prepara el staging con la
red deshabilitada. Para crear y verificar el ZIP desde el host, prepara primero
las mismas dependencias con `make deps-exts-linux` y después ejecuta
`make release-linux` y `make validate-artifact-linux`.

Las rutas por defecto son `.build/linux-l4d2/` y `dist/linux/artifact/`.
Se conservan los alias Linux `deps-linux`, `build-linux` y `package-linux`.
