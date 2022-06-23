add_library_unity(
        duckdb_func_geospatial
        OBJECT
        spatial_predicates.cpp)
set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:duckdb_func_geospatial>
        PARENT_SCOPE)
