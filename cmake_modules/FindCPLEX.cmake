SET(CPLEX_HEADER_LIST
   ilcplex/ilocplex.h
   ilconcert/ilomodel.h
)

SET(CPLEX_LIBRARY_LIST
   ilocplex
   cplex
   concert
)

SET(CPLEX_FOUND_HEADERS TRUE)
FOREACH(HEADER ${CPLEX_HEADER_LIST})
    FIND_PATH(CPLEX_PATH_${HEADER}
        NAMES ${HEADER}
    )
    IF(CPLEX_PATH_${HEADER})
        SET(CPLEX_INCLUDE_DIR ${CPLEX_INCLUDE_DIR} ${CPLEX_PATH_${HEADER}})
    ELSE(CPLEX_PATH_${HEADER})
        SET(CPLEX_FOUND_HEADERS FALSE)
    ENDIF(CPLEX_PATH_${HEADER})
ENDFOREACH(HEADER)

IF(CPLEX_FOUND_HEADERS)
    MESSAGE(STATUS "Found CPLEX include dir: ${CPLEX_INCLUDE_DIR}")
ELSE(CPLEX_FOUND_HEADERS)
    MESSAGE(STATUS "Could not find CPLEX include dir")
ENDIF(CPLEX_FOUND_HEADERS)

SET(CPLEX_FOUND_LIBRARIES TRUE)
FOREACH(LIBRARY ${CPLEX_LIBRARY_LIST})
    FIND_LIBRARY(CPLEX_LIB_${LIBRARY}
        NAMES ${LIBRARY}
    )
    IF(CPLEX_LIB_${LIBRARY})
        SET(CPLEX_LIBRARIES ${CPLEX_LIBRARIES} ${CPLEX_LIB_${LIBRARY}})
    ELSE(CPLEX_LIB_${LIBRARY})
        SET(CPLEX_FOUND_LIBRARIES FALSE)
    ENDIF(CPLEX_LIB_${LIBRARY})
ENDFOREACH(LIBRARY)

IF(CPLEX_FOUND_LIBRARIES)
    MESSAGE(STATUS "Found CPLEX libraries: ${CPLEX_LIBRARIES}")
ELSE(CPLEX_FOUND_LIBRARIES)
    MESSAGE(STATUS "Could not find CPLEX libraries")
ENDIF(CPLEX_FOUND_LIBRARIES)


IF(CPLEX_FOUND_HEADERS AND CPLEX_FOUND_LIBRARIES)
  SET(CPLEX_FOUND TRUE)
ENDIF(CPLEX_FOUND_HEADERS AND CPLEX_FOUND_LIBRARIES)
