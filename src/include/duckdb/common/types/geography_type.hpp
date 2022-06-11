#pragma once

#include "duckdb/common/constants.hpp"

#include <array>
#include <numeric>
#include <vector>

namespace duckdb {


enum class GeographyType : uint8_t {
	POINT = 0,
	LINESTRING = 1,
	POLYGON = 2,
	MULTIPOINT = 3,
	MULTILINESTRING = 4,
	MULTIPOLYGON = 6,
	GEOMETRY_COLLECTION = 7,
	UNKNOWN = 250
};

class WKTWriter;

class Geography {
	friend WKTWriter;

public:
	Geography() = delete;

	// Point
	Geography(double *lngs, double *lats)
	    : type(GeographyType::POINT), lngs_head(lngs), lats_head(lats), lines_len({1}) { }

	// Linestring
	Geography(const GeographyType geog_type, double *lngs, double *lats, idx_t line_len)
	    : type(geog_type), lngs_head(lngs), lats_head(lats), lines_len({line_len}) { }

	// Polygon, Multipoint, Multilinestring
	Geography(const GeographyType geog_type, double *lngs, double *lats, const std::vector<idx_t> &lines_len)
	    : type(geog_type), lngs_head(lngs), lats_head(lats), lines_len(lines_len) { }

	// Multipolygon
	Geography(const GeographyType geog_type, double *lngs, double *lats, const std::vector<idx_t> &lines_len,
	          const std::vector<idx_t> &multi_len)
	    : type(geog_type), lngs_head(lngs), lats_head(lats), lines_len(lines_len), multi_len(multi_len) { }

	// Geography Collection
	Geography(const GeographyType geog_type, double *lngs, double *lats, const std::vector<idx_t> &lines_len,
	          const std::vector<idx_t> &multi_len, const std::vector<idx_t> &coll_mpoly_len,
	          const std::vector<GeographyType> &coll_types)
	    : type(geog_type), lngs_head(lngs), lats_head(lats), lines_len(lines_len), multi_len(multi_len),
	      coll_mpoly_len(coll_mpoly_len), coll_types(coll_types) { }

	// Deep Copy constructor
	Geography(GeographyType type, double *lngs, double *lats, const std::vector<idx_t> &lines_len,
	          const std::vector<idx_t> &multi_len, const std::vector<idx_t> &coll_mpoly_len,
	          const std::vector<GeographyType> &coll_types, bool owns_coords)
	    : type(type), lngs_head(lngs), lats_head(lats), lines_len(lines_len), multi_len(multi_len),
	      coll_mpoly_len(coll_mpoly_len), coll_types(coll_types), owns_coords(owns_coords) { }

	// Empty Geography
	explicit Geography(GeographyType type) : type(type), lngs_head(nullptr), lats_head(nullptr) { }

	Geography(const Geography &geography)
	    : type(geography.type), lngs_head(geography.lngs_head), lats_head(geography.lats_head),
	      lines_len(geography.lines_len), multi_len(geography.multi_len),
	      coll_mpoly_len(geography.coll_mpoly_len), coll_types(geography.coll_types),
	      owns_coords(geography.owns_coords) {
		if (owns_coords) {
			AllocAndCopyToSelf(geography);
		}
	}

	Geography(Geography&& geography) noexcept
	    : type(geography.type), lngs_head(geography.lngs_head), lats_head(geography.lats_head),
	      lines_len(std::move(geography.lines_len)), multi_len(std::move(geography.multi_len)),
	      coll_mpoly_len(std::move(geography.coll_mpoly_len)), coll_types(std::move(geography.coll_types)),
	      owns_coords(geography.owns_coords) {
		geography.owns_coords = false;
	}

	Geography& operator=(Geography& other);
	Geography& operator=(Geography&& other)  noexcept;

	~Geography() {
		FreeCoords();
	}

	//! Comparison operators
	bool operator==(const Geography &rhs) const {
		return Geography::Equals(*this, rhs);
	}

	bool operator!=(const Geography &rhs) const {
		return !Geography::Equals(*this, rhs);
	}

	bool operator<(const Geography &rhs) const {
		throw std::runtime_error("Comparison operator < is not supported by the Geography type.");
	}

	bool operator<=(const Geography &rhs) const {
		throw std::runtime_error("Comparison operator <= is not supported by the Geography type.");
	}

	bool operator>(const Geography &rhs) const {
		throw std::runtime_error("Comparison operator > is not supported by the Geography type.");
	}

	bool operator>=(const Geography &rhs) const {
		throw std::runtime_error("Comparison operator >= is not supported by the Geography type.");
	}

	static bool Equals(const Geography &left, const Geography &right);
	static Geography CopyDeep(const Geography &other);

	GeographyType GetType() const {
		return type;
	}

	const std::vector<idx_t> &GetLinesLen() const {
		return lines_len;
	}

	const std::vector<idx_t> &GetMultiLen() const {
		return multi_len;
	}

	double *GetLngs() const {
		return lngs_head;
	}
	double *GetLats() const {
		return lats_head;
	}

	void SetCoords(double *lngs, double *lats) {
		FreeCoords();
		lngs_head = lngs;
		lats_head = lats;
	}

	static std::string GeographyTypeToString(GeographyType type);

	inline idx_t NumPoints() const;

private:
	static std::array<double*, 2> AllocAndCopy(const Geography &from);
	void AllocAndCopyToSelf(const Geography &from);
	void FreeCoords();

private:
	GeographyType type;

	double *lngs_head;
	double *lats_head;

	// Repetition levels for different object types
	std::vector<idx_t> lines_len;		// Number of points
	std::vector<idx_t> multi_len;		// Number of lines
	std::vector<idx_t> coll_mpoly_len;	// Number of polygons

	std::vector<GeographyType> coll_types;

	// By default, lngs, lats arrays are pointers to the VectorBuffer managed memory.
	// For deep copy, coord arrays are copied and instance of Geography then owns the coord arrays.
	bool owns_coords = false;
};

} // namespace duckdb
