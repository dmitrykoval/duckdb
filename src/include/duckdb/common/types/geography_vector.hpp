//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/types/geography_vector.hpp
//
//
//===----------------------------------------------------------------------===//

#include "duckdb/common/types/vector.hpp"
#include "s2/s2latlng.h"
#include "s2/s2loop.h"
#include "s2/s2polygon.h"

#pragma once

namespace duckdb {

class S2ObjectFactory {
public:
	template <GeographyType GEO_T, typename S2_TYPE>
	static S2_TYPE GetS2Object(const Geography &geography) {
		throw InternalException("GetS2Objects is not implemented for given geography_type " +
		                        Geography::GeographyTypeToString(GEO_T));
	}

	template <>
	S2Point GetS2Object<GeographyType::POINT, S2Point>(const Geography &geography) {
		return S2LatLng::FromDegrees(*geography.GetLats(), *geography.GetLngs()).ToPoint();
	}

	template <>
	S2Polyline GetS2Object<GeographyType::LINESTRING, S2Polyline>(const Geography &geography) {
		const auto *lats = geography.GetLats();
		const auto *lngs = geography.GetLngs();

		std::vector<S2LatLng> vertices;
		auto lines_len = geography.GetLinesLen();
		for (auto line_len : lines_len) {
			for (idx_t l = 0; l < line_len; l++) {
				vertices.emplace_back(S2LatLng::FromDegrees(*lats++, *lngs++));
			}
		}

		return S2Polyline(vertices);
	}

	template <>
	S2Polygon GetS2Object<GeographyType::POLYGON, S2Polygon>(const Geography &geography) {
		const auto *lats = geography.GetLats();
		const auto *lngs = geography.GetLngs();
		auto lines_it = geography.GetLinesLen().cbegin();

		return S2Polygon(CreatePolygon(&lats, &lngs, lines_it, geography.GetLinesLen().size()));
	}

	template <>
	std::vector<S2Polygon>
	GetS2Object<GeographyType::MULTIPOLYGON, std::vector<S2Polygon>>(const Geography &geography) {
		const auto *lats = geography.GetLats();
		const auto *lngs = geography.GetLngs();

		std::vector<S2Polygon> polygons;

		auto lines_it = geography.GetLinesLen().cbegin();
		for (idx_t num_lines : geography.GetMultiLen()) {
			polygons.emplace_back(CreatePolygon(&lats, &lngs, lines_it, num_lines));
		}

		return polygons;
	}

private:
	static std::vector<std::unique_ptr<S2Loop>> CreatePolygon(const double **lats_pt, const double **lngs_pt,
	                                                          std::vector<idx_t>::const_iterator &lines_it,
	                                                          const idx_t num_lines) {
		const double *lats = *lats_pt;
		const double *lngs = *lngs_pt;

		size_t points_added = 0;
		std::vector<std::unique_ptr<S2Loop>> loops;
		for (idx_t i = 0; i < num_lines; i++, lines_it++) {
			std::vector<S2Point> loop_points;
			auto line_len = *lines_it;
			for (idx_t l = 0; l < line_len; l++, points_added++) {
				loop_points.push_back(S2LatLng::FromDegrees(*lats++, *lngs++).ToPoint());
			}
			auto loop = new S2Loop(loop_points);
			loop->Normalize();
			loops.emplace_back(loop);
		}

		*lats_pt += points_added;
		*lngs_pt += points_added;

		return loops;
	}
};

class GeographyBuffer : public VectorBuffer {
public:
	GeographyBuffer() : VectorBuffer(VectorBufferType::GEOGRAPHY_BUFFER) {
		NewChunk(MIN_BUFFER_SIZE);
	}

public:
	std::array<double *, 2> AddPoint(double lng, double lat) {
		EnsureEnoughSpace(1);

		auto &lngs_head = lng_chunks[curr_chunk];
		auto &lats_head = lat_chunks[curr_chunk];
		auto start_idx = curr_in_chunk_pos;
		lngs_head[curr_in_chunk_pos] = lng;
		lats_head[curr_in_chunk_pos] = lat;
		curr_in_chunk_pos++;

		return {&lngs_head[start_idx], &lats_head[start_idx]};
	}

	std::array<double *, 2> AddPoints(const std::vector<double> &lngs, const std::vector<double> &lats) {
		const idx_t len = lngs.size();
		EnsureEnoughSpace(len);

		auto &lngs_head = lng_chunks[curr_chunk];
		auto &lats_head = lat_chunks[curr_chunk];
		auto start_idx = curr_in_chunk_pos;
		for (idx_t i = 0; i < len; i++, curr_in_chunk_pos++) {
			lngs_head[curr_in_chunk_pos] = lngs[i];
			lats_head[curr_in_chunk_pos] = lats[i];
		}

		return {&lngs_head[start_idx], &lats_head[start_idx]};
	}

	std::array<double *, 2> AddPoints(const double *lngs, const double *lats, idx_t len) {
		EnsureEnoughSpace(len);

		auto &lngs_head = lng_chunks[curr_chunk];
		auto &lats_head = lat_chunks[curr_chunk];

		auto start_idx = curr_in_chunk_pos;
		std::memcpy(&lngs_head[curr_in_chunk_pos], lngs, len);
		std::memcpy(&lats_head[curr_in_chunk_pos], lats, len);
		curr_in_chunk_pos += len;

		return {&lngs_head[start_idx], &lats_head[start_idx]};
	}

	GeographyType GetGeoType() const {
		return geo_type;
	}

	void UpdateGeoType(GeographyType type) {
		// If all Geographies are of the same type, geo_type would be set to that type.
		// If Geographies are of mixed type, geo_type would be set to collection, reflecting mixed type
		if (geo_type == GeographyType::UNKNOWN) {
			geo_type = type;
		} else if (geo_type != type){
			geo_type = GeographyType::GEOMETRY_COLLECTION;
		}
	}

private:
	void NewChunk(const idx_t len) {
		auto const chunk_size = MaxValue<idx_t>(len, MIN_BUFFER_SIZE);
		lng_chunks.emplace_back(unique_ptr<double[]>(new double[chunk_size]));
		lat_chunks.emplace_back(unique_ptr<double[]>(new double[chunk_size]));
		sizes.push_back(chunk_size);

		curr_chunk++;
		curr_in_chunk_pos = 0;
	}

	void EnsureEnoughSpace(const idx_t len) {
		if (sizes[curr_chunk] - curr_in_chunk_pos < len) {
			NewChunk(len);
		}
	}

private:
	vector<unique_ptr<double[]>> lng_chunks;
	vector<unique_ptr<double[]>> lat_chunks;
	vector<idx_t> sizes;

	row_t curr_chunk = -1;
	idx_t curr_in_chunk_pos = 0;

	GeographyType geo_type = GeographyType::UNKNOWN;

	constexpr static idx_t MIN_BUFFER_SIZE = 4096;
};

struct GeographyVector {

	template <GeographyType GEO_T, typename S2_TYPE>
	class Iterator {
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = S2_TYPE;
		using pointer = S2_TYPE *;
		using reference = S2_TYPE &;

	public:
		explicit Iterator(Vector &vector) {
			geography = (Geography *)FlatVector::GetData(vector);
		}

		S2_TYPE operator*() {
			return S2ObjectFactory::GetS2Object<GEO_T, S2_TYPE>(*geography);
		}
		//		pointer operator->() { return geo; }

		Iterator &operator++() {
			geography++;
			return *this;
		}

		Iterator operator++(int) {
			Iterator tmp = *this;
			++(*this);
			return tmp;
		}

		friend bool operator==(const Iterator &a, const Iterator &b) {
			return a.geo == b.geo;
		};
		friend bool operator!=(const Iterator &a, const Iterator &b) {
			return a.geo != b.geo;
		};

	private:
		Geography *geography;
	};

	//! Add a Point
	static Geography &AddPoint(Vector &vector, idx_t idx, double lng, double lat);

	//! Add a generic Geography
	template <typename... Args>
	static Geography &AddGeography(Vector &vector, idx_t idx, GeographyType geog_type, const std::vector<double> &lngs,
	                               const std::vector<double> &lats, Args &&...args) {
		D_ASSERT(vector.GetType().id() == LogicalTypeId::GEOGRAPHY);

		if (lngs.empty()) {
			return AddEmpty(vector, idx, geog_type);
		}

		if (!vector.auxiliary) {
			vector.auxiliary = make_buffer<GeographyBuffer>();
		}
		D_ASSERT(vector.auxiliary->GetBufferType() == VectorBufferType::GEOGRAPHY_BUFFER);

		auto &geography_buffer = (GeographyBuffer &)*vector.auxiliary;
		auto coords = geography_buffer.AddPoints(lngs, lats);

		auto data_ptr = FlatVector::GetData<Geography>(vector);
		auto geo_obj =
		    new (data_ptr + idx) Geography(geog_type, coords[0], coords[1], std::forward<Args>(args)...);
		geography_buffer.UpdateGeoType(geo_obj->GetType());

		return data_ptr[idx];
	}

	static Geography &AddEmpty(Vector &vector, idx_t idx, GeographyType type);

	template <GeographyType GEO_T, typename S2_TYPE>
	static Iterator<GEO_T, S2_TYPE> GetS2ObjectsIterator(Vector &vector) {
		D_ASSERT(vector.GetType().id() == LogicalTypeId::GEOGRAPHY);
		D_ASSERT(GetGeoType(vector) == GEO_T);

		return Iterator<GEO_T, S2_TYPE>(vector);
	}

	static void CopyGeography(const Vector &from_vector, idx_t from_idx, Vector &to_vector, idx_t to_idx);
	static GeographyType GetGeoType(const Vector &vector);
	static bool IsGeoType(const Vector &vector, const GeographyType geo_type);
};

class GeographyVectorWriter {
public:
	explicit GeographyVectorWriter(Vector &vector) : vector(vector), current_idx(0) {
	}

	//! Add a Point
	Geography &AddPoint(double lng, double lat);

	//! Add a Geography
	template <typename... Args>
	Geography &AddGeography(Args &&...args) {
		return GeographyVector::AddGeography(vector, current_idx++, std::forward<Args>(args)...);
	}

	//! Add an Empty Geography
	Geography &AddEmpty(GeographyType type);

	//! Add next Null
	void AddNull();

private:
	Vector &vector;
	idx_t current_idx = 0;
};

} // namespace duckdb
