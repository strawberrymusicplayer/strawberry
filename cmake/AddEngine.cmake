# Strawberry Music Player
# Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
#
# Strawberry is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Strawberry is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.

macro(add_engine engine_lower engine_upper lib_list src_list inc_list enabled)

  #message(STATUS "ADD ENGINE: ${engine_lower} ${engine_upper} ${lib_list} ${src_list} ${inc_list} ${enabled}")
  
  #set(ENGINE_LIBRARIES "")

  # recreate list
  set(lib_list ${lib_list})
  #list(GET lib_list 0 name)

  # add a user selectable build option
  option(ENGINE_${engine_upper}_ENABLED "enable engine ${engine_upper}" ${enabled})

  # check if engine is enabled and needed librares are available
  if(ENGINE_${engine_upper}_ENABLED)

    # check for all needed libraries
    foreach(lib ${lib_list})
      #pkg_check_modules(${lib} ${lib})
      if (NOT ${lib}_FOUND MATCHES 1)
        set(ENGINE_${engine_upper}_LIB_MISSING TRUE)
      endif(NOT ${lib}_FOUND MATCHES 1)
    endforeach(lib ${lib_list})

    if(ENGINE_${engine_upper}_LIB_MISSING)
      set(ENGINES_MISSING "${ENGINES_MISSING} ${engine_lower}")
      #set("HAVE_${engine_upper}" 0 CACHE INTERNAL ${engine_upper})
      set("HAVE_${engine_upper}" OFF)
    else(ENGINE_${engine_upper}_LIB_MISSING)
      # add define -DHAVE_<engine> so we can clutter the code with #ifdefs
      #set("HAVE_${engine_upper}" 1 CACHE INTERNAL ${engine_upper})
      set("HAVE_${engine_upper}" ON)
      # add sources and headers
      list(APPEND SOURCES ${src_list})
      list(APPEND HEADERS ${inc_list})
      # add libraries to link against
      foreach(lib ${lib_list})
        #set(ENGINE_LIBRARIES ${ENGINE_LIBRARIES} ${${lib}_LIBRARIES} CACHE INTERNAL libraries)
        set(ENGINE_LIBRARIES ${ENGINE_LIBRARIES} ${${lib}_LIBRARIES})
      endforeach(lib ${lib_list})
      # add to list of enabled engines
      set(ENGINES_ENABLED "${ENGINES_ENABLED} ${engine_lower}")
    endif(ENGINE_${engine_upper}_LIB_MISSING)
  else(ENGINE_${engine_upper}_ENABLED)
    set(ENGINES_DISABLED "${ENGINES_DISABLED} ${engine_lower}")
    #set("HAVE_${engine_upper}" 0 CACHE INTERNAL ${engine_upper})
    set("HAVE_${engine_upper}" OFF)
  endif(ENGINE_${engine_upper}_ENABLED)

endmacro(add_engine engine_lower engine_upper lib_list src_list inc_list enabled)

# print engines to be built
macro(print_engines)

  if(ENGINES_ENABLED)
    message(STATUS "Building engines:${ENGINES_ENABLED}")
  endif(ENGINES_ENABLED)
  if(ENGINES_DISABLED)
    message(STATUS "Disabled engines:${ENGINES_DISABLED}")
  endif(ENGINES_DISABLED)
  if(ENGINES_MISSING)
    message(STATUS "Missing engines:${ENGINES_MISSING}")
  endif(ENGINES_MISSING)

  #message(STATUS "Engine libraries:${ENGINE_LIBRARIES}")

  # need at least 1 engine
  if(NOT ENGINES_ENABLED)
    message(FATAL_ERROR "No engine enabled!")
  endif(NOT ENGINES_ENABLED)

endmacro(print_engines)
