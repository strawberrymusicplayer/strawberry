set(summary_willbuild "")
set(summary_willnotbuild "")

macro(optional_component_summary_add name test)
  if (${test})
    list(APPEND summary_willbuild ${name})
  else (${test})
    list(APPEND summary_willnotbuild "${name}")
  endif (${test})
endmacro(optional_component_summary_add)

macro(optional_component_summary_show_part variable title)
  list(LENGTH ${variable} _len)
  if (_len)
    message("")
    message(${title})
    foreach (_item ${${variable}})
      message("   ${_item}")
    endforeach (_item)
  endif (_len)
endmacro(optional_component_summary_show_part)

macro(optional_component_summary_show)
  list(SORT summary_willbuild)
  list(SORT summary_willnotbuild)
  message("")
  message("Building strawberry version: ${STRAWBERRY_VERSION_DISPLAY}, Qt version ${Qt${QT_VERSION_MAJOR}Core_VERSION}")
  optional_component_summary_show_part(summary_willbuild "The following components will be built:")
  optional_component_summary_show_part(summary_willnotbuild "The following components WILL NOT be built:")
  message("")
endmacro(optional_component_summary_show)

function(optional_component name default description)

  set(option_variable "ENABLE_${name}")
  set(have_variable "HAVE_${name}")
  set(${have_variable} OFF)

  # Create the option
  option(${option_variable} "${description}" ${default})

  # Was the option set?
  if(NOT ${option_variable})
    set(summary_willnotbuild "${summary_willnotbuild};${description} (disabled in CMake config)" PARENT_SCOPE)
    return()
  endif()

  # Check each of the dependencies
  set(next_arg_is_dep_name FALSE)
  set(testing_deps TRUE)
  set(current_dep_name)
  set(missing_deps)

  foreach(arg ${ARGN})
    if(${next_arg_is_dep_name})
      set(current_dep_name "${arg}")
      set(next_arg_is_dep_name FALSE)
    elseif(arg STREQUAL "DEPENDS")
      set(next_arg_is_dep_name TRUE)
      set(testing_deps TRUE)
    elseif(${testing_deps})
      string(REPLACE " " ";" arglist "${arg}")
      if(${arglist})
        # We have to do this instead of if(NOT ${arg}) so that tests may contain "NOT" themselves.
      else()
        list(APPEND missing_deps "${current_dep_name}")
        set(testing_deps FALSE)
      endif()
    endif()
  endforeach()

  if(missing_deps)
    foreach(dep ${missing_deps})
      if(deplist_text)
        set(deplist_text "${deplist_text}, ${dep}")
      else()
        set(deplist_text "${dep}")
      endif()
    endforeach()
    set(text "${description} (missing ${deplist_text})")

    set(summary_willnotbuild "${summary_willnotbuild};${text}" PARENT_SCOPE)

    message(FATAL_ERROR "${text}, to disable this optional feature, pass -D${option_variable}=OFF to CMake")

  else()
    set(${have_variable} ON PARENT_SCOPE)
    set(summary_willbuild "${summary_willbuild};${description}" PARENT_SCOPE)
  endif()
endfunction()