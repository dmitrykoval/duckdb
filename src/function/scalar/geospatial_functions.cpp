#include "duckdb/function/scalar/geospatial_functions.hpp"

namespace duckdb {

void BuiltinFunctions::RegisterGeospatialFunctions() {
	Register<GeoFromWKTFun>();
	Register<GeoMakePointFun>();
	Register<GeoContainsFun>();
	Register<GeoWithinFun>();
}

} // namespace duckdb
