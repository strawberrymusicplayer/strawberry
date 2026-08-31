set(ACOUSTID_API_KEY "" CACHE STRING "AcoustID API key")
set(LASTFM_API_KEY "" CACHE STRING "Last.fm API key")
set(LASTFM_SHARED_SECRET "" CACHE STRING "Last.fm shared secret")
set(LISTENBRAINZ_CLIENT_ID "" CACHE STRING "ListenBrainz OAuth client ID")
set(LISTENBRAINZ_CLIENT_SECRET "" CACHE STRING "ListenBrainz OAuth client secret")
set(OPENTIDAL_CLIENT_ID "" CACHE STRING "OpenTidal OAuth client ID")
set(OPENTIDAL_CLIENT_SECRET "" CACHE STRING "OpenTidal OAuth client secret")
set(SPOTIFY_CLIENT_ID "" CACHE STRING "Spotify OAuth client ID")
set(SPOTIFY_CLIENT_SECRET "" CACHE STRING "Spotify OAuth client secret")
set(GENIUS_CLIENT_ID "" CACHE STRING "Genius OAuth client ID")
set(GENIUS_CLIENT_SECRET "" CACHE STRING "Genius OAuth client secret")
set(MUSIXMATCH_APP_ID "" CACHE STRING "Musixmatch Android app_id")
set(MUSIXMATCH_APP_SECRET "" CACHE STRING "Musixmatch Android HMAC-SHA1 signature secret")
set(DISCOGS_CLIENT_ID "" CACHE STRING "Discogs consumer key")
set(DISCOGS_CLIENT_SECRET "" CACHE STRING "Discogs consumer secret")
set(TIDAL_CLIENT_ID "" CACHE STRING "Tidal OAuth client ID")

mark_as_advanced(
  ACOUSTID_API_KEY
  LASTFM_API_KEY
  LASTFM_SHARED_SECRET
  LISTENBRAINZ_CLIENT_ID
  LISTENBRAINZ_CLIENT_SECRET
  OPENTIDAL_CLIENT_ID
  OPENTIDAL_CLIENT_SECRET
  SPOTIFY_CLIENT_ID
  SPOTIFY_CLIENT_SECRET
  GENIUS_CLIENT_ID
  GENIUS_CLIENT_SECRET
  MUSIXMATCH_APP_ID
  MUSIXMATCH_APP_SECRET
  DISCOGS_CLIENT_ID
  DISCOGS_CLIENT_SECRET
  TIDAL_CLIENT_ID
)

set(API_CREDENTIALS_ENCRYPTION_KEY "" CACHE STRING "Passphrase to AES-256-CBC-encrypt embedded API credentials with, instead of storing them in plaintext")
mark_as_advanced(API_CREDENTIALS_ENCRYPTION_KEY)

if(API_CREDENTIALS_ENCRYPTION_KEY)
  find_program(OPENSSL_EXECUTABLE openssl)
  if(NOT OPENSSL_EXECUTABLE)
    message(FATAL_ERROR "API_CREDENTIALS_ENCRYPTION_KEY is set, but the openssl command-line tool was not found - it is required at configure time to encrypt the credentials. Install it, or unset API_CREDENTIALS_ENCRYPTION_KEY to store credentials in plaintext instead.")
  endif()

  # SHA-256 of the passphrase is used directly as the raw 256-bit AES key - matches what Utilities::MaybeDecryptApiCredential() (src/utilities/cryptutils.cpp) derives from the same passphrase at runtime.
  string(SHA256 api_credentials_encryption_key_hex "${API_CREDENTIALS_ENCRYPTION_KEY}")
endif()

function(_encrypt_api_credential name)

  if("${${name}}" STREQUAL "" OR "${API_CREDENTIALS_ENCRYPTION_KEY}" STREQUAL "")
    return()
  endif()

  execute_process(
    COMMAND ${OPENSSL_EXECUTABLE} rand -hex 16
    OUTPUT_VARIABLE iv_hex
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE openssl_result
  )
  if(NOT openssl_result EQUAL 0)
    message(FATAL_ERROR "Failed to generate a random IV with openssl while encrypting the ${name} API credential.")
  endif()

  execute_process(
    COMMAND ${CMAKE_COMMAND} -E echo_append "${${name}}"
    COMMAND ${OPENSSL_EXECUTABLE} enc -aes-256-cbc -K ${api_credentials_encryption_key_hex} -iv ${iv_hex} -a -A
    OUTPUT_VARIABLE ciphertext_base64
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE openssl_result
  )
  if(NOT openssl_result EQUAL 0)
    message(FATAL_ERROR "Failed to encrypt the ${name} API credential with openssl.")
  endif()

  set(${name} "ENC:${iv_hex}:${ciphertext_base64}" PARENT_SCOPE)

endfunction()

_encrypt_api_credential(ACOUSTID_API_KEY)
_encrypt_api_credential(LASTFM_API_KEY)
_encrypt_api_credential(LASTFM_SHARED_SECRET)
_encrypt_api_credential(LISTENBRAINZ_CLIENT_ID)
_encrypt_api_credential(LISTENBRAINZ_CLIENT_SECRET)
_encrypt_api_credential(OPENTIDAL_CLIENT_ID)
_encrypt_api_credential(OPENTIDAL_CLIENT_SECRET)
_encrypt_api_credential(SPOTIFY_CLIENT_ID)
_encrypt_api_credential(SPOTIFY_CLIENT_SECRET)
_encrypt_api_credential(GENIUS_CLIENT_ID)
_encrypt_api_credential(GENIUS_CLIENT_SECRET)
_encrypt_api_credential(MUSIXMATCH_APP_ID)
_encrypt_api_credential(MUSIXMATCH_APP_SECRET)
_encrypt_api_credential(DISCOGS_CLIENT_ID)
_encrypt_api_credential(DISCOGS_CLIENT_SECRET)
_encrypt_api_credential(TIDAL_CLIENT_ID)

option(CREATE_SOURCE_API_CREDENTIALS "Generate src/apicredentials.h into the source tree instead of the build tree (needed so it lands in the release source tarball)" OFF)

if(CREATE_SOURCE_API_CREDENTIALS)
  configure_file(${CMAKE_SOURCE_DIR}/src/apicredentials.h.in ${CMAKE_SOURCE_DIR}/src/apicredentials.h @ONLY)
else()
  configure_file(${CMAKE_SOURCE_DIR}/src/apicredentials.h.in ${CMAKE_BINARY_DIR}/src/apicredentials.h @ONLY)
endif()
