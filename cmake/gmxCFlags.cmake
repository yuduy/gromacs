
# Test C flags FLAGS, and set VARIABLE to true if the work. Also add the
# flags to CFLAGSVAR.
MACRO(GMX_TEST_CFLAG VARIABLE FLAGS CFLAGSVAR)
    IF(NOT DEFINED ${VARIABLE})
        CHECK_C_COMPILER_FLAG("${FLAGS}" ${VARIABLE})
        IF (${VARIABLE})
            SET (${CFLAGSVAR} "${FLAGS} ${${CFLAGSVAR}}")
        ENDIF (${VARIABLE}) 
    ENDIF(NOT DEFINED ${VARIABLE})
ENDMACRO(GMX_TEST_CFLAG VARIABLE FLAGS CFLAGSVAR)

# Test C++ flags FLAGS, and set VARIABLE to true if the work. Also add the
# flags to CXXFLAGSVAR.
MACRO(GMX_TEST_CXXFLAG VARIABLE FLAGS CXXFLAGSVAR)
    IF(NOT DEFINED ${VARIABLE})
        CHECK_CXX_COMPILER_FLAG("${FLAGS}" ${VARIABLE})
        IF (${VARIABLE})
            SET (${CXXFLAGSVAR} "${FLAGS} ${${CXXFLAGSVAR}}")
        ENDIF (${VARIABLE}) 
    ENDIF(NOT DEFINED ${VARIABLE})
ENDMACRO(GMX_TEST_CXXFLAG VARIABLE FLAGS CXXFLAGSVAR)


# This is the actual exported function to be called 
MACRO(gmx_c_flags)

    include(CheckCCompilerFlag)
    include(CheckCXXCompilerFlag)

    # gcc
    if(CMAKE_COMPILER_IS_GNUCC)
        GMX_TEST_CFLAG(CFLAGS_WARN "-Wall -Wno-unused" GMXC_CFLAGS)
        # new in gcc 4.5
        GMX_TEST_CFLAG(CFLAGS_EXCESS_PREC "-fexcess-precision=fast" GMXC_CFLAGS)
        GMX_TEST_CFLAG(CFLAGS_COPT "-fomit-frame-pointer -finline-functions -funroll-all-loops" 
                       GMXC_CFLAGS_RELEASE)
    endif()
    # g++
    if(CMAKE_COMPILER_IS_GNUCXX)
        GMX_TEST_CXXFLAG(CXXFLAGS_WARN "-Wall -Wno-unused" GMXC_CXXFLAGS)
      # new in gcc 4.5
        GMX_TEST_CXXFLAG(CXXFLAGS_EXCESS_PREC "-fexcess-precision=fast" 
                          GMXC_CXXFLAGS)
        GMX_TEST_CXXFLAG(CXXFLAGS_COPT "-fomit-frame-pointer -finline-functions -funroll-all-loops" 
                         GMXC_CXXFLAGS_RELEASE)
    endif()

    # icc
    if (CMAKE_C_COMPILER_ID MATCHES "Intel")
        if (NOT WIN32) 
            GMX_TEST_CFLAG(CFLAGS_OPT "-ip -w -funroll-all-loops -std=gnu99" 
                            GMXC_CFLAGS)
            GMX_TEST_CFLAG(CFLAGS_SSE2 "-msse2" GMXC_CFLAGS)
            GMX_TEST_CFLAG(CFLAGS_X86 "-mtune=core2" GMXC_CFLAGS_RELEASE)
            GMX_TEST_CFLAG(CFLAGS_IA64 "-mtune=itanium2" GMXC_CFLAGS_RELEASE)
        else()
            GMX_TEST_CFLAG(CFLAGS_SSE2 "/arch:SSE2" GMXC_CFLAGS)
            GMX_TEST_CFLAG(CFLAGS_X86 "/Qip" GMXC_CFLAGS_RELEASE)
        endif()
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
        if (NOT WIN32) 
            GMX_TEST_CXXFLAG(CXXFLAGS_OPT 
                             "-ip -w -funroll-all-loops -std=gnu99" 
                             GMXC_CXXFLAGS)
            GMX_TEST_CXXFLAG(CXXFLAGS_SSE2 "-msse2" GMXC_CXXFLAGS)
            GMX_TEST_CXXFLAG(CXXFLAGS_X86 "-mtune=core2" GMXC_CXXFLAGS_RELEASE)
            GMX_TEST_CXXFLAG(CXXFLAGS_IA64 "-mtune=itanium2" 
                              GMXC_CXXFLAGS_RELEASE)
        else()
            GMX_TEST_CXXFLAG(CXXFLAGS_SSE2 "/arch:SSE2" GMXC_CXXFLAGS)
            GMX_TEST_CXXFLAG(CXXFLAGS_X86 "/Qip" GMXC_CXXFLAGS_RELEASE)
        endif()
    endif()

    # pgi
    if (CMAKE_C_COMPILER_ID MATCHES "PGI")
        GMX_TEST_CFLAG(CFLAGS_OPT "-fastsse" GMXC_CFLAGS)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "PGI")
        GMX_TEST_CXXFLAG(CXXFLAGS_OPT "-fastsse" GMXC_CXXFLAGS)
    endif()

    # Pathscale: There is currently no good way to test for this one.
    #if (CMAKE_CXX_COMPILER_ID MATCHES "Pathscale")
    #    GMX_TEST_CXXFLAG(CXXFLAGS_OPT "-OPT:Ofast -fno-math-errno -ffast-math" 
    #                     GMXC_CXXFLAGS)
    #endif()

    # xlc
    if (CMAKE_C_COMPILER_ID MATCHES "XL")
        GMX_TEST_CFLAG(CFLAGS_OPT "-qarch=auto -qtune=auto" GMXC_CFLAGS)
        GMX_TEST_CFLAG(CFLAGS_LANG "-qlanglvl=extc99" GMXC_CFLAGS)
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "XL")
        GMX_TEST_CXXFLAG(CFLAGS_OPT "-qarch=auto -qtune=auto" GMXC_CXXFLAGS)
    endif()


    # now actually set the flags:
    if (NOT DEFINED GMXCFLAGS_SET)
        set(GMXCFLAGS_SET true CACHE INTERNAL "Whether to reset the C flags" 
            FORCE)
        # C
        set(CMAKE_C_FLAGS "${GMXC_CFLAGS} ${CMAKE_C_FLAGS}" 
            CACHE STRING "Flags used by the compiler during all build types" 
            FORCE)
        set(CMAKE_C_FLAGS_RELEASE "${GMXC_CFLAGS_RELEASE} 
            ${CMAKE_C_FLAGS_RELEASE}" 
            CACHE STRING "Flags used by the compiler during release builds" 
            FORCE)
        # C++
        set(CMAKE_CXX_FLAGS "${GMXC_CXXFLAGS} ${CMAKE_CXX_FLAGS}" 
            CACHE STRING "Flags used by the compiler during all build types" 
            FORCE)
        set(CMAKE_CXX_FLAGS_RELEASE 
            "${GMXC_CXXFLAGS_RELEASE} ${CMAKE_CXX_FLAGS_RELEASE}" 
            CACHE STRING "Flags used by the compiler during release builds" 
            FORCE)
    endif()
ENDMACRO(gmx_c_flags)

