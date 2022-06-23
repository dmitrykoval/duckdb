#pragma once

#include "duckdb/common/types/geography_type.hpp"

namespace duckdb {

Geography &Geography::operator=(Geography &other) {
	if (&other != this) {
		type = other.type;

		lines_len = other.lines_len;
		multi_len = other.multi_len;
		coll_mpoly_len = other.coll_mpoly_len;

		coll_types = other.coll_types;

		owns_coords = other.owns_coords;

		if (owns_coords && lngs_head == nullptr) {
			owns_coords = false;
		}

		if (owns_coords) {
			AllocAndCopyToSelf(other);
		} else {
			lngs_head = other.lngs_head;
			lats_head = other.lats_head;
		}
	}

	return *this;
}

Geography &Geography::operator=(Geography &&other) noexcept {
	type = other.type;

	lngs_head = other.lngs_head;
	lats_head = other.lats_head;

	lines_len = std::move(other.lines_len);
	multi_len = std::move(other.multi_len);
	coll_mpoly_len = std::move(other.coll_mpoly_len);

	coll_types = std::move(other.coll_types);

	owns_coords = other.owns_coords;
	other.owns_coords = false;

	return *this;
}

//! Comparison operators
bool Geography::Equals(const Geography &left, const Geography &right) {
	if (&left == &right) {
		return true;
	}

	auto num_points = left.NumPoints();
	if (num_points != right.NumPoints()) {
		return false;
	}

	return left.type == right.type && left.lines_len == right.lines_len && left.multi_len == right.multi_len &&
	       left.coll_mpoly_len == right.coll_mpoly_len && left.coll_types == right.coll_types &&
	       std::equal(left.lngs_head, left.lngs_head + num_points, right.lngs_head) &&
	       std::equal(left.lats_head, left.lats_head + num_points, right.lats_head);
}

Geography Geography::CopyDeep(const Geography &other) {
	double *lngs_copy = nullptr, *lats_copy = nullptr;
	bool made_copy = false;
	if (other.lngs_head != nullptr) {
		auto copy_c = AllocAndCopy(other);
		lngs_copy = copy_c[0];
		lats_copy = copy_c[1];
		made_copy = true;
	}

	return Geography(other.type, lngs_copy, lats_copy, other.lines_len, other.multi_len, other.coll_mpoly_len,
	                 other.coll_types, made_copy);
}

std::string Geography::GeographyTypeToString(GeographyType type) {
	switch (type) {
	case GeographyType::POINT:
		return "POINT";
	case GeographyType::LINESTRING:
		return "LINESTRING";
	case GeographyType::POLYGON:
		return "POLYGON";
	case GeographyType::MULTIPOINT:
		return "MULTIPOINT";
	case GeographyType::MULTILINESTRING:
		return "MULTILINESTRING";
	case GeographyType::MULTIPOLYGON:
		return "MULTIPOLYGON";
	case GeographyType::GEOMETRY_COLLECTION:
		return "GEOMETRYCOLLECTION";
	case GeographyType::UNKNOWN:
		return "UNKNOWN";
	default:
		throw std::invalid_argument("Unsupported Geography type.");
	}
}

inline idx_t Geography::NumPoints() const {
	return std::accumulate(lines_len.begin(), lines_len.end(), static_cast<idx_t>(0));
}

std::array<double *, 2> Geography::AllocAndCopy(const Geography &from) {
	auto const size = from.NumPoints();
	if (size == 0) {
		return {nullptr, nullptr};
	}

	double *lngs = new double[size * 2];
	double *lats = lngs + size;

	auto const size_bytes = size * sizeof(double);
	std::memcpy(lngs, from.lngs_head, size_bytes);
	std::memcpy(lats, from.lats_head, size_bytes);

	return {lngs, lats};
}

void Geography::AllocAndCopyToSelf(const Geography &from) {
	auto copy_c = AllocAndCopy(from);
	lngs_head = copy_c[0];
	lats_head = copy_c[1];
}

void Geography::FreeCoords() {
	if (owns_coords) {
		// Coordinate arrays owned by the Geography object, have contiguous memory layout.
		// Only one allocation is needed for all arrays and therefore only one delete.
		delete (lngs_head);
	}
}

} // namespace duckdb
