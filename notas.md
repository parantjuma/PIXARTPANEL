1327
listarArchivosGif()  <= aqui se realizan todas las llamadas
                            - mantenemos el tema de la firma de directorios
                            

ok - loadGifCache()     => ahora es buildGifIndexFixedArray()
    => esto lo que haria es cargar la cache array de offsets al fichero cache



ok- 

    => debe hacer el escaneo como hasta ahora, pero no cargar en memoria, si no escribir
        en un archivo. Unicamente se modifica la linea donde escribe

hay que añadir funciones para acceso a archivo segun offset

        getGifPathByIndex(uint32_t index) 


    modificar en los accesos a archivo (void ejecutarModoGif() )
    - Abrir un archivo secuencial ** este podria tener mejoras de busqueda y punteros
    - Abrir un archivo aleatorio 


    buscar cuando hace uso del acceso secuencial y aleatorio de ficheros para sustituir

    en la subida de archivos hay que incluir una recarga de los offsets y añadir un archivo mas
    a la cache de ficheros al final con el nuevo fichero.
    los offsets no habra otra posibilidad que borrar la estructura de memoria para añadir un nuevo archivo. Esto puede ocasionar que se fragmente de forma incorrecta la memoria. Para prevenir esto podemos crear siempre el array de offsets con 100 entradas mas por si acaso y un contador
    con posiciones extras

    ver como solucionar la eliminacion de archivos  (poner a null la posición en offset por ejemplo y no devolverla o controlar
    en la funcion de solicitud )

archivosGIF.empty()             => isGifOffsetEmpty()
archivosGIF.size()              => gifOffsetSize()
archivosGIF[currentGifIndex]    => getGifPathByIndex(uint32_t index)

