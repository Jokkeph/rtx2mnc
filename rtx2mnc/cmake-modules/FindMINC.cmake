# FindMINC.cmake module


FIND_PATH(MINC_INCLUDE_DIR minc2.h /opt/minc/include /usr/include /usr/local/include /usr/local/bic/include /opt/minc/1.9.15/include /opt/minc/1.9.15)
FIND_LIBRARY(MINC_minc2_LIBRARY NAMES minc2 HINTS /opt/minc/lib /usr/lib /usr/local/lib /usr/local/bic/lib /opt/minc/1.9.15 /opt/minc/1.9.15/lib)


IF (MINC_INCLUDE_DIR AND MINC_minc2_LIBRARY)
   set(MINC_LIBRARIES
     ${MINC_minc2_LIBRARY}
     )
   SET(MINC_FOUND TRUE)
   
ENDIF (MINC_INCLUDE_DIR AND MINC_minc2_LIBRARY)


IF (MINC_FOUND)
   IF (NOT Minc_FIND_QUIETLY)
      MESSAGE(STATUS "Found MINC: ${MINC_LIBRARIES}")
   ENDIF (NOT Minc_FIND_QUIETLY)
ELSE (MINC_FOUND)
   IF (Minc_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find MINC")
   ENDIF (Minc_FIND_REQUIRED)
ENDIF (MINC_FOUND)


mark_as_advanced(
  MINC_minc2_LIBRARY
)
