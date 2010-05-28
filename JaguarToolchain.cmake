# the name of the target operating system
SET(CMAKE_SYSTEM_NAME CRAYXT_COMPUTE_LINUX)

# set the compiler
set(CMAKE_C_COMPILER cc -target=linux)
set(CMAKE_CXX_COMPILER CC -target=linux)

# set the search path for the environment coming with the compiler
# and a directory where you can install your own compiled software
set(CMAKE_FIND_ROOT_PATH
   /opt/xt-pe/default
   /opt/mpt/3.5.0/xt/mpich2-gnu/
   /ccs/home/kmorel/local
   /ccs/home/work/stow/paraview-slac-jaguarpf-v3.8.0-rc2
 )

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

