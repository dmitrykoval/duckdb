#include "duckdb/common/spatial/WKTWriter.hpp"

#include <vector>

namespace duckdb {

std::string WKTWriter::GeogToWkt(const Geography &geog) {
	if (geog.lngs_head == nullptr) {
		return "EMPTY";
	}

	std::stringstream out;

	auto lngs_it = geog.lngs_head;
	auto lats_it = geog.lats_head;
	auto lines_it = geog.lines_len.cbegin();
	auto multi_it = geog.multi_len.cbegin();

	if (geog.type == GeographyType::GEOMETRY_COLLECTION) {
		WriteGeogCollection(out, geog, &lngs_it, &lats_it, lines_it, multi_it);
	} else {
		WriteGeogObject(out, geog, geog.type, false, &lngs_it, &lats_it, lines_it, multi_it, geog.multi_len.size());
	}

	return out.str();
}

void WKTWriter::WriteGeogObject(std::stringstream &out, const Geography &geog, GeographyType type, bool in_collection,
                                double **lngs_it, double **lats_it, std::vector<idx_t>::const_iterator &lines_it,
                                std::vector<idx_t>::const_iterator &multi_it, idx_t multi_polys_len) {
	out << Geography::GeographyTypeToString(type);

	switch (type) {
	case GeographyType::POINT: {
		WriteLines(out, lngs_it, lats_it, lines_it, 1, false);
		break;
	}
	case GeographyType::LINESTRING: {
		WriteLines(out, lngs_it, lats_it, lines_it, 1, false);
		break;
	}
	case GeographyType::POLYGON: {
		auto const len = in_collection ? *(multi_it++) : geog.lines_len.size();
		WriteLines(out, lngs_it, lats_it, lines_it, len, true);
		break;
	}
	case GeographyType::MULTIPOINT: {
		auto const len = in_collection ? *(multi_it++) : geog.lines_len.size();
		WriteLines(out, lngs_it, lats_it, lines_it, len, true);
		break;
	}
	case GeographyType::MULTILINESTRING: {
		auto const len = in_collection ? *(multi_it++) : geog.lines_len.size();
		WriteLines(out, lngs_it, lats_it, lines_it, len, true);
		break;
	}
	case GeographyType::MULTIPOLYGON: {
		WritePolygons(out, lngs_it, lats_it, lines_it, multi_it, multi_polys_len);
		break;
	}
	case GeographyType::GEOMETRY_COLLECTION:
		throw std::invalid_argument("Nested Geography Collections are not supported.");
	default:
		throw std::invalid_argument("Unsupported Geography type.");
	}
}

void WKTWriter::WriteGeogCollection(std::stringstream &out, const Geography &geog, double **lngs_it, double **lats_it,
                                    std::vector<idx_t>::const_iterator &lines_it,
                                    std::vector<idx_t>::const_iterator &multi_it) {
	out << Geography::GeographyTypeToString(geog.type) << "(";

	auto multi_polys_it = geog.coll_mpoly_len.cbegin();
	auto mpoly_len = geog.multi_len.size();
	for (size_t i = 0, len = geog.coll_types.size(); i < len; i++) {
		auto type = geog.coll_types[i];
		if (type == GeographyType::MULTIPOLYGON) {
			mpoly_len = *(multi_polys_it++);
		}
		WriteGeogObject(out, geog, type, true, lngs_it, lats_it, lines_it, multi_it, mpoly_len);
		if (i != len - 1) {
			out << ", ";
		}
	}

	out << ")";
}

void WKTWriter::WriteCoords(std::stringstream &out, double **lngs_it, double **lats_it, idx_t len, bool enclose_coords) {
	if (enclose_coords) { out << "("; }
	for (idx_t i = 0; i < len; i++, (*lngs_it)++, (*lats_it)++) {
		out << **lngs_it << " " << **lats_it;
		if (i != len - 1) {
			out << ", ";
		}
	}
	if (enclose_coords) { out << ")"; }
}

void WKTWriter::WriteLines(std::stringstream &out, double **lngs_it, double **lats_it,
                           std::vector<idx_t>::const_iterator &lines_it, idx_t len, bool enclose_coords) {
	out << "(";
	for (idx_t i = 0; i < len; i++, lines_it++) {
		WriteCoords(out, lngs_it, lats_it, *lines_it, enclose_coords);
		if (i != len - 1) {
			out << ", ";
		}
	}
	out << ")";
}

void WKTWriter::WritePolygons(std::stringstream &out, double **lngs_it, double **lats_it,
                              std::vector<idx_t>::const_iterator &lines_it,
                              std::vector<idx_t>::const_iterator &multi_it, idx_t len) {
	out << "(";
	for (idx_t i = 0; i < len; i++, multi_it++) {
		WriteLines(out, lngs_it, lats_it, lines_it, *multi_it, true);
		if (i != len - 1) {
			out << ", ";
		}
	}
	out << ")";
}

} // namespace duckdb
