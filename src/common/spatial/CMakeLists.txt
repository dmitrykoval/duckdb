add_library_unity(
  duckdb_spatial
  OBJECT
  WKTReader.cpp
  WKTWriter.cpp
  StringTokenizer.cpp)

set(ALL_OBJECT_FILES
    ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_spatial>
    PARENT_SCOPE)
