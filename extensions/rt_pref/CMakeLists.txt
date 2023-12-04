# The rt_pref extension
PROJECT("Increase preference of peers based on Application ID or relay status for a given message" C)

# List of source files
SET(RT_PREF_SRC
	rt_pref.c
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Compile these files as a freeDiameter extension
FD_ADD_EXTENSION(rt_pref ${RT_PREF_SRC})

####
## INSTALL section ##

# We install with the daemon component because it is a base feature.
INSTALL(TARGETS rt_pref
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-daemon)
