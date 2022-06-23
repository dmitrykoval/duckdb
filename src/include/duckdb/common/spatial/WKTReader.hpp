#pragma once

#include "duckdb/common/spatial/StringTokenizer.hpp"
#include "duckdb/common/types/geography_vector.hpp"
#include "duckdb/common/types/geography_type.hpp"
#include "duckdb/common/types/vector.hpp"

#include <string>

namespace duckdb {

class WKTReader {

public:
	explicit WKTReader(GeographyVectorWriter &writer) : writer(writer) {}
	void Read(const std::string &wkt);

private:
	void ReadGeography(StringTokenizer &tokenizer);

	std::tuple<double, double, bool> ReadPoint(StringTokenizer &tokenizer) const;
	uint64_t ReadLinestring(StringTokenizer &tokenizer, std::vector<double> &lngs,
	                        std::vector<double> &lats) const;
	uint32_t ReadPolygon(StringTokenizer &tokenizer, std::vector<double> &lngs, std::vector<double> &lats,
	                     std::vector<idx_t> &lines_len) const;

	void ReadPointFromCollection(StringTokenizer &tokenizer, std::vector<double> &lngs,
	                             std::vector<double> &lats) const;
	uint64_t ReadMultiPoint(StringTokenizer &tokenizer, std::vector<double> &lngs,
	                        std::vector<double> &lats) const;
	uint32_t ReadMultiLinestring(StringTokenizer &tokenizer, std::vector<double> &lngs, std::vector<double> &lats,
	                         std::vector<idx_t> &lines_len) const;
	uint32_t ReadMultiPolygon(StringTokenizer &tokenizer, std::vector<double> &lngs, std::vector<double> &lats,
	                      std::vector<idx_t> &lines_len, std::vector<idx_t> &multi_len) const;

	void ReadGeometryCollection(StringTokenizer &tokenizer, std::vector<double> &lngs,
	                            std::vector<double> &lats, std::vector<idx_t> &lines_len,
	                            std::vector<idx_t> &multi_len, std::vector<GeographyType> &col_types,
	                            std::vector<idx_t> &coll_mpoly_len) const;

	void ReadCollectionContent(StringTokenizer &tokenizer, std::vector<double> &lngs, std::vector<double> &lats,
	                           std::vector<idx_t> &lines_len, std::vector<idx_t> &multi_len,
	                           std::vector<GeographyType> &col_types, std::vector<idx_t> &coll_mpoly_len) const;

	uint64_t ReadCoordinates(StringTokenizer &tokenizer, std::vector<double> &lngs,
	                         std::vector<double> &lats) const;

	std::pair<double, double> NextCoord(StringTokenizer &tokenizer) const;
	uint32_t ReadLinesCoords(StringTokenizer &tokenizer, std::vector<double> &lngs, std::vector<double> &lats,
	                         std::vector<idx_t> &lines_len) const;

private:
	GeographyVectorWriter &writer;
};

} // namespace duckdb
