# Use this file in the -C flag of cmake to initialize the settings for
# compiling on Jaguar.

# These special compiler commands are actually wrappers for compiling on CNL.
SET(CMAKE_CXX_COMPILER CC CACHE STRING "")
SET(CMAKE_C_COMPILER cc CACHE STRING "")

# It is important to build static.  The compute nodes do not support dynamic
# libraries.
SET(BUILD_SHARED_LIBS OFF CACHE BOOL "")

# Turn on release compile.
SET(CMAKE_BUILD_TYPE Release CACHE STRING "")

SET(CMAKE_C_FLAGS "-static" CACHE STRING "")
SET(CMAKE_CXX_FLAGS "-static" CACHE STRING "")

SET(CMAKE_EXE_LINKER_FLAGS "-static" CACHE STRING "")

# Tell cmake to use the .a (static) libraries.  The real shared libraries
# (.so) will not work on the compute nodes.
#SET(CMAKE_STATIC_LIBRARY_PREFIX "lib" CACHE STRING "")
#SET(CMAKE_STATIC_LIBRARY_SUFFIX ".a" CACHE STRING "")
#SET(CMAKE_SHARED_LIBRARY_PREFIX "lib" CACHE STRING "")
#SET(CMAKE_SHARED_LIBRARY_SUFFIX ".a" CACHE STRING "")

# Do not need to build the GUI.
SET(PARAVIEW_BUILD_QT_GUI OFF CACHE BOOL "")

# Threads not supported on CNL.
SET(CMAKE_USE_PTHREADS OFF CACHE BOOL "")
SET(CMAKE_THREAD_LIBS "" CACHE STRING "")

# Build with OSMesa so that we do not need X11.
SET(VTK_OPENGL_HAS_OSMESA ON CACHE BOOL "")
SET(OSMESA_LIBRARY /ccs/home/kmorel/local/lib/libOSMesa.a CACHE FILEPATH "")
SET(OSMESA_INCLUDE_DIR /ccs/home/kmorel/local/include CACHE PATH "")
SET(OPENGL_glu_LIBRARY /ccs/home/kmorel/local/lib/libGLU.a CACHE FILEPATH "")
SET(OPENGL_gl_LIBRARY "" CACHE FILEPATH "")
SET(VTK_USE_X OFF CACHE BOOL "")

# Set up MPI.  I found a directory for MPI in my MPICH_DIR environment
# variable (presumably set up with the PrgEnv-* module).
SET(PARAVIEW_USE_MPI ON CACHE BOOL "")
SET(MPI_LIBRARY "/opt/cray/mpt/4.0.0/xt/seastar/mpich2-gnu/lib/libmpich.a"
  CACHE FILEPATH "")
SET(MPI_INCLUDE_PATH "/opt/cray/mpt/4.0.0/xt/seastar/mpich2-gnu/include"
  CACHE PATH "")
SET(MPI_EXTRA_LIBRARY "" CACHE FILEPATH "")

# Where I want "make install" to put things.
SET(CMAKE_INSTALL_PREFIX /ccs/home/kmorel/work/local/stow/ParaView3.8.0-RC2-Jaguar
  CACHE PATH "")

# Don't need testing.
SET(BUILD_TESTING OFF CACHE BOOL "")
