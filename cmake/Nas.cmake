include(FetchContent)

function(strawberry_add_smb2)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  set(BUILD_SHARED_LIBS OFF)
  set(ENABLE_EXAMPLES OFF)
  set(ENABLE_LIBKRB5 OFF)
  set(ENABLE_GSSAPI OFF)
  set(LIBKRB5_FOUND FALSE)
  set(GSSAPI_FOUND FALSE)
  set(HAVE_LIBKRB5 FALSE)
  set(HAVE_GSSAPI_GSSAPI_H FALSE)
  set(core_DEPENDS "")
  FetchContent_Declare(strawberry_smb2
    URL https://codeload.github.com/sahlberg/libsmb2/tar.gz/refs/tags/libsmb2-6.3
    URL_HASH SHA256=5011003fd091513f897e1ebbb8e1a40af634386df7199716348b7f77569a2790
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(strawberry_smb2)
  set_target_properties(smb2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_property(TARGET smb2 PROPERTY SYSTEM TRUE)
  if(ANDROID)
    configure_file("${strawberry_smb2_SOURCE_DIR}/COPYING" "${CMAKE_BINARY_DIR}/android-build/assets/licenses/libsmb2-COPYING" COPYONLY)
    configure_file("${strawberry_smb2_SOURCE_DIR}/LICENCE-LGPL-2.1.txt" "${CMAKE_BINARY_DIR}/android-build/assets/licenses/libsmb2-LGPL-2.1.txt" COPYONLY)
  endif()
endfunction()

strawberry_add_smb2()
