# Compiles the Java sources and runs the test class; used by ctest for the Java tests of Android code that does not need the Android SDK.
# Variables: JAVAC, JAVA, OUTPUT_DIR, TEST_CLASS and SOURCES (a list separated by |).
string(REPLACE "|" ";" SOURCES "${SOURCES}")
file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
execute_process(COMMAND "${JAVAC}" -Xlint:all -Werror -d "${OUTPUT_DIR}" ${SOURCES} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Compiling ${TEST_CLASS} failed")
endif()
execute_process(COMMAND "${JAVA}" -cp "${OUTPUT_DIR}" "${TEST_CLASS}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "${TEST_CLASS} failed")
endif()
