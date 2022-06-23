#pragma once

#include "duckdb/common/types/geography_type.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/common/types/geography_vector.hpp"

#include <string>

namespace duckdb {

class WKTWriter {
public:
	static std::string GeogToWkt(const Geography &geog);

private:
	static void WriteGeogObject(std::stringstream &out, const Geography &geog, GeographyType type,
	                            bool in_collection, double **lngs_it, double **lats_it,
	                            std::vector<idx_t>::const_iterator &lines_it,
	                            std::vector<idx_t>::const_iterator &multi_it, idx_t multi_polys_len);
	static void WriteGeogCollection(std::stringstream &out, const Geography &geog, double **lngs_it, double **lats_it,
	                                std::vector<idx_t>::const_iterator &lines_it,
	                                std::vector<idx_t>::const_iterator &multi_it);
	static void WriteCoords(std::stringstream &out, double **lngs_it, double **lats_it, idx_t len, bool enclose_coords);
	static void WriteLines(std::stringstream &out, double **lngs_it, double **lats_it,
	                       std::vector<idx_t>::const_iterator &lines_it, idx_t len, bool enclose_coords=false);
	static void WritePolygons(std::stringstream &out, double **lngs_it, double **lats_it,
	                          std::vector<idx_t>::const_iterator &lines_it,
	                          std::vector<idx_t>::const_iterator &multi_it, idx_t len);
};

} // namespace duckdb
