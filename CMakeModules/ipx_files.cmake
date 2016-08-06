# Add files to global property shared among all CMake files
#
# Param: _var   Property name
# Param: ...    Variable list of files
function(ipx_files_add _var)
	foreach(_src ${ARGN})
		get_filename_component(_abs "${_src}" ABSOLUTE)
		set_property(GLOBAL APPEND PROPERTY "${_var}" "${_abs}")
	endforeach()
endfunction()

# Get files from global property shared among all CMake files
# All files will have paths relative to the current source directory
#
# Param: _dst      Local destination variable
# Param: _glob_var Property name
function(ipx_files_get _dst _glob_var)
	set(_loc "")
	get_property(_tmp GLOBAL PROPERTY ${_glob_var})
	foreach(_src ${_tmp})
		file(RELATIVE_PATH _relPath ${CMAKE_CURRENT_SOURCE_DIR} ${_src})
		list(APPEND _loc "${_relPath}")
	endforeach()
	set(${_dst} ${_loc} PARENT_SCOPE)
endfunction()
