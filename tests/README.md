# Pruebas de SourceTV-Manager

`make test-ci` comprueba procedencia de dependencias, esquema y contenido
del paquete, y el rechazo de entradas inválidas antes de publicar. Usa checkouts
temporales y dobles del publicador, sin descargas ni Sonar.

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
