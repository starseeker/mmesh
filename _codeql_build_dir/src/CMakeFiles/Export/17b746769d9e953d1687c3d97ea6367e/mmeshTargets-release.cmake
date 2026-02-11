#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "mmesh::mmesh" for configuration "Release"
set_property(TARGET mmesh::mmesh APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(mmesh::mmesh PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmmesh.so"
  IMPORTED_SONAME_RELEASE "libmmesh.so"
  )

list(APPEND _cmake_import_check_targets mmesh::mmesh )
list(APPEND _cmake_import_check_files_for_mmesh::mmesh "${_IMPORT_PREFIX}/lib/libmmesh.so" )

# Import target "mmesh::mmesh-static" for configuration "Release"
set_property(TARGET mmesh::mmesh-static APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(mmesh::mmesh-static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmmesh.a"
  )

list(APPEND _cmake_import_check_targets mmesh::mmesh-static )
list(APPEND _cmake_import_check_files_for_mmesh::mmesh-static "${_IMPORT_PREFIX}/lib/libmmesh.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
