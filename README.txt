
UDF para Algoritmo de PCA
Postgres + LAPACK

Inspirado en :
Carlos Ordonez, Naveen Mohanam, Carlos Garcia-Alvarado. PCA for Large Data Sets with Parallel Data Summarization, Distributed and Parallel Databases (DAPD), 32(3):377-403, 2014, Springer, special issue on Big Data Analytics.
http://www2.cs.uh.edu/~ordonez/pdfwww/w-2014-DAPD-udfsvd.pdf


Pasos

1.  Apoyense de dgesvd_ex.c para utilizar un ejemplo de pca en lapacke.
    Vean el tutorial svd http://web.mit.edu/be.400/www/SVD/Singular_Value_Decomposition.htm

2.  Apoyense de p2_iris_create_tables.sql para cargar la base de datos IRIS
    en postgres.

3.  p2_pca_udf es una udf en postgres que recibe dos cadenas como argumento:
    la primera es el nombre de una tabla en la base de datos y
    la segunda es una cadena con los nombres de las columnas de dicha tabla
    para llevar a cabo el algoritmo de pca sobre ellas.

4.  Apoyense de p2_udf_lapack_notes.txt para compilar + cargar + usar la
    udf de pca con lapack.
    IMPORTANTE Para utilizar lapack en una udf hay que COMPILARLO de tal
    manera que se pueda utilizar como una biblioteca compartida (shared library)
    ver aqui como: http://icl.cs.utk.edu/lapack-forum/viewtopic.php?f=7&t=908
    MEJOR recomiendo instalarlo mediante apt-get
    sudo apt-get install libblas-dev
    sudo apt-get install libblas-doc
    sudo apt-get install liblapacke-dev
    sudo apt-get install liblapack-doc

5.  Selection_001.png y Selection_002.png son imagenes del uso de la udf:
    como se devuelven los resultados actualmente y como se deben interpretar.
    Notese que los resultados concuerdan con los obtenidos en
    https://rpubs.com/njvijay/27823

6.  Como ejercicio queda pendiente mejorar la udf aqui propuesta.
    Hay varios problemas pendientes aun, entre ellos:
  6.1 ¿De que manera se puede hacer para devolver los resultados de PCA
      en una tabla, en lugar de una cadena (que es como actualmente se hace)?
  6.2 Incrementar los datos en la base de datos a fin de observar
      de manera detallada el desempeño de una udf con respecto a otras
      herramientas (por ejemplo R).
      Usar (generar) 50,000 o 100,000 registros o de plano cambiarse a tpch.
      Anexar reporte con los tiempos obtenidos.
  6.3 Manejo de errores, ¿que pasa cuando se meten datos incorrectos?
      Recuerden que pca solo acepta valores numericos,
      ¿que pasa cuando hay un error de sistema dentro de la udf?
      ¿como nos recuperamos?
      ¿Como avisamos al usuario este tipo de eventos?
  6.4 En mi codigo, utilizo estructuras de datos de mas..  todo con el fin de
      hacer el procedimiento lo mas claro posible :)
      ¿de que manera se puede optimizar?
