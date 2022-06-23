#include "duckdb/common/spatial/WKTReader.hpp"
#include "duckdb/common/spatial/StringTokenizer.hpp"

#include <string>
#include <vector>

namespace duckdb {

void WKTReader::Read(const std::string &wkt) {
	StringTokenizer tokenizer(wkt);
	ReadGeography(tokenizer);
}

void WKTReader::ReadGeography(StringTokenizer &tokenizer) {
	std::string type = tokenizer.getNextWord();

	// Point has a special case, which doesn't require initialization of two coordinate vectors.
	// Third boolean element in a tuple indicates if the coordinate was a valid one (true) or an empty one (false).
	if (type == "POINT") {
		auto point_coord = ReadPoint(tokenizer);
		if (std::get<2>(point_coord)) {
			writer.AddPoint(std::get<0>(point_coord), std::get<1>(point_coord));
		} else {
			writer.AddEmpty(GeographyType::POINT);
		}
		return;
	}

	std::vector<double> lngs;
	std::vector<double> lats;
	std::vector<idx_t> lines_len;

	if (type == "LINESTRING") {
		ReadLinestring(tokenizer, lngs, lats);
		writer.AddGeography(GeographyType::LINESTRING, lngs, lats, lngs.size());
	} else if (type == "POLYGON") {
		ReadPolygon(tokenizer, lngs, lats, lines_len);
		writer.AddGeography(GeographyType::POLYGON, lngs, lats, lines_len);
	} else if (type == "MULTIPOINT") {
		ReadMultiPoint(tokenizer, lngs, lats);
		writer.AddGeography(GeographyType::MULTIPOINT, lngs, lats,
		                    std::vector<idx_t>(lngs.size(), 1), std::vector<idx_t>{lngs.size()});
	} else if (type == "MULTILINESTRING") {
		ReadMultiLinestring(tokenizer, lngs, lats, lines_len);
		writer.AddGeography(GeographyType::MULTILINESTRING, lngs, lats, lines_len);
	} else if (type == "MULTIPOLYGON") {
		std::vector<idx_t> multi_len;
		ReadMultiPolygon(tokenizer, lngs, lats, lines_len, multi_len);
		writer.AddGeography(GeographyType::MULTIPOLYGON, lngs, lats, lines_len, multi_len);
	} else if (type == "GEOMETRYCOLLECTION") {
		std::vector<idx_t> multi_len;
		std::vector<GeographyType> col_types;
		std::vector<idx_t> coll_mpoly_len;
		ReadGeometryCollection(tokenizer, lngs, lats, lines_len, multi_len, col_types, coll_mpoly_len);
		writer.AddGeography(GeographyType::GEOMETRY_COLLECTION, lngs, lats, lines_len, multi_len,
		                    coll_mpoly_len, col_types);
	} else {
		throw std::invalid_argument("Geography type '" + type + "' is not supported.");
	}
}

std::tuple<double, double, bool> WKTReader::ReadPoint(StringTokenizer &tokenizer) const {
	std::string next_token = tokenizer.getNextEmptyOrOpener();
	if (next_token == "EMPTY") {
		return std::make_tuple(0, 0, false);
	}

	auto coord = NextCoord(tokenizer);
	tokenizer.getNextCloser();

	return std::make_tuple(coord.first, coord.second, true);
}

uint64_t WKTReader::ReadLinestring(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                   std::vector<double> &lats) const {
	return ReadCoordinates(tokenizer, lngs, lats);
}

uint32_t WKTReader::ReadPolygon(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                std::vector<double> &lats, std::vector<idx_t> &lines_len) const {
	return ReadLinesCoords(tokenizer, lngs, lats, lines_len);
}

void WKTReader::ReadPointFromCollection(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                        std::vector<double> &lats) const {
	std::string next_token = tokenizer.getNextEmptyOrOpener();
	if (next_token == "EMPTY") {
		return;
	}

	auto coord = NextCoord(tokenizer);
	lngs.push_back(coord.first);
	lats.push_back(coord.second);
	tokenizer.getNextCloser();
}

uint64_t WKTReader::ReadMultiPoint(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                   std::vector<double> &lats) const {
	std::string next_token = tokenizer.getNextEmptyOrOpener();
	if (next_token == "EMPTY") {
		return 0;
	}

	int token = tokenizer.peekNextToken();
	bool grouped_coords = (token == '(');
	if (token == StringTokenizer::TT_NUMBER || token == '(') {
		uint64_t coords_added = 0;
		do {
			if (grouped_coords) {
				// skip next opener or stop the iteration if there's none.
				if (tokenizer.getNextEmptyOrOpener() == "EMPTY") {
					break;
				}
			}

			auto coords = NextCoord(tokenizer);
			lngs.push_back(coords.first);
			lats.push_back(coords.second);
			coords_added++;

			if (grouped_coords) {
				tokenizer.getNextCloserOrComma();
			}

			next_token = tokenizer.getNextCloserOrComma();
		} while (next_token == ",");
		return coords_added;
	} else {
		throw std::invalid_argument("Unexpected token.");
	}
}

uint32_t WKTReader::ReadLinesCoords(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                    std::vector<double> &lats, std::vector<idx_t> &lines_len) const {
	std::string next_token = tokenizer.getNextEmptyOrOpener();
	if (next_token == "EMPTY") {
		return 0;
	}

	uint32_t lines_read = 0;
	do {
		auto coords_added = ReadCoordinates(tokenizer, lngs, lats);
		lines_len.push_back(coords_added);
		next_token = tokenizer.getNextCloserOrComma();
		lines_read++;
	} while (next_token == ",");

	return lines_read;
}

uint32_t WKTReader::ReadMultiLinestring(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                        std::vector<double> &lats, std::vector<idx_t> &lines_len) const {
	return ReadLinesCoords(tokenizer, lngs, lats, lines_len);
}

uint32_t WKTReader::ReadMultiPolygon(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                     std::vector<double> &lats, std::vector<idx_t> &lines_len,
                                     std::vector<idx_t> &multi_len) const {
	std::string next_token = tokenizer.getNextEmptyOrOpener();
	if (next_token == "EMPTY") {
		return 0;
	}

	uint32_t polygons_cnt = 0;
	do {
		auto lines_read = ReadLinesCoords(tokenizer, lngs, lats, lines_len);
		multi_len.push_back(lines_read);
		next_token = tokenizer.getNextCloserOrComma();
		polygons_cnt++;
	} while (next_token == ",");

	return polygons_cnt;
}

void WKTReader::ReadGeometryCollection(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                       std::vector<double> &lats, std::vector<idx_t> &lines_len,
                                       std::vector<idx_t> &multi_len, std::vector<GeographyType> &col_types,
                                       std::vector<idx_t> &coll_mpoly_len) const {
	std::string next_token = tokenizer.getNextEmptyOrOpener();

	do {
		ReadCollectionContent(tokenizer, lngs, lats, lines_len, multi_len, col_types, coll_mpoly_len);
		next_token = tokenizer.getNextCloserOrComma();

	} while (next_token == ",");
}

void WKTReader::ReadCollectionContent(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                      std::vector<double> &lats, std::vector<idx_t> &lines_len,
                                      std::vector<idx_t> &multi_len, std::vector<GeographyType> &col_types,
                                      std::vector<idx_t> &coll_mpoly_len) const {
	std::string type = tokenizer.getNextWord();

	if (type == "POINT") {
		ReadPointFromCollection(tokenizer, lngs, lats);
		lines_len.push_back(1);
		col_types.push_back(GeographyType::POINT);
	} else if (type == "LINESTRING") {
		auto line_len = ReadLinestring(tokenizer, lngs, lats);
		lines_len.push_back(line_len);
		col_types.push_back(GeographyType::LINESTRING);
	} else if (type == "POLYGON") {
		auto lines_added = ReadPolygon(tokenizer, lngs, lats, lines_len);
		multi_len.push_back(lines_added);
		col_types.push_back(GeographyType::POLYGON);
	} else if (type == "MULTIPOINT") {
		auto coords_added = ReadMultiPoint(tokenizer, lngs, lats);
		for (uint64_t i = 0; i < coords_added; i++) {
			lines_len.push_back(1);
		}
		multi_len.push_back(coords_added);
		col_types.push_back(GeographyType::MULTIPOINT);
	} else if (type == "MULTILINESTRING") {
		auto lines_added = ReadMultiLinestring(tokenizer, lngs, lats, lines_len);
		multi_len.push_back(lines_added);
		col_types.push_back(GeographyType::MULTILINESTRING);
	} else if (type == "MULTIPOLYGON") {
		auto polys_cnt = ReadMultiPolygon(tokenizer, lngs, lats, lines_len, multi_len);
		col_types.push_back(GeographyType::MULTIPOLYGON);
		coll_mpoly_len.push_back(polys_cnt);
	} else if (type == "GEOMETRYCOLLECTION") {
		throw std::invalid_argument("Nested Geography collections are not supported.");
	} else {
		throw std::invalid_argument("Geography type '" + type + "' is not supported.");
	}
}

std::pair<double, double> WKTReader::NextCoord(StringTokenizer &tokenizer) const {
	auto lng = tokenizer.getNextNumber();
	auto lat = tokenizer.getNextNumber();
	if (tokenizer.isNumberNext()) {
		throw std::range_error("Only 2-dimensional geography coordinates are supported. (WGS84 lng, lat)");
	}

	return std::make_pair(lng, lat);
}

uint64_t WKTReader::ReadCoordinates(StringTokenizer &tokenizer, std::vector<double> &lngs,
                                    std::vector<double> &lats) const {
	std::string next_token = tokenizer.getNextEmptyOrOpener();
	if (next_token == "EMPTY") {
		return 0;
	}

	uint64_t coords_added = 0;
	do {
		auto coords = NextCoord(tokenizer);
		lngs.push_back(coords.first);
		lats.push_back(coords.second);
		next_token = tokenizer.getNextCloserOrComma();
		coords_added++;
	} while (next_token == ",");

	return coords_added;
}

} // namespace duckdb
