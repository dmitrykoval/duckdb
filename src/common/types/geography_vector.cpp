#pragma once

#include "duckdb/common/types/geography_vector.hpp"

namespace duckdb {

Geography &GeographyVector::AddPoint(Vector &vector, idx_t idx, double lng, double lat) {
	D_ASSERT(vector.GetType().id() == LogicalTypeId::GEOGRAPHY);

	if (!vector.auxiliary) {
		vector.auxiliary = make_buffer<GeographyBuffer>();
	}
	D_ASSERT(vector.auxiliary->GetBufferType() == VectorBufferType::GEOGRAPHY_BUFFER);

	auto &geography_buffer = (GeographyBuffer &)*vector.auxiliary;
	auto coords = geography_buffer.AddPoint(lng, lat);
	geography_buffer.UpdateGeoType(GeographyType::POINT);

	auto data_ptr = FlatVector::GetData<Geography>(vector);
	new (data_ptr + idx) Geography(coords[0], coords[1]);
	return data_ptr[idx];
}

Geography &GeographyVector::AddEmpty(Vector &vector, idx_t idx, GeographyType type) {
	D_ASSERT(vector.GetType().id() == LogicalTypeId::GEOGRAPHY);

	auto data_ptr = FlatVector::GetData<Geography>(vector);
	new (data_ptr + idx) Geography(type);
	return data_ptr[idx];
}

void GeographyVector::CopyGeography(const Vector &from_vector, idx_t from_idx, Vector &to_vector, idx_t to_idx) {
	D_ASSERT(from_vector.GetType().id() == LogicalTypeId::GEOGRAPHY);
	D_ASSERT(to_vector.GetType().id() == LogicalTypeId::GEOGRAPHY);

	auto from_ptr = FlatVector::GetData<Geography>(from_vector);
	auto to_ptr = FlatVector::GetData<Geography>(to_vector);

	if (!to_vector.auxiliary) {
		to_vector.auxiliary = make_buffer<GeographyBuffer>();
	}
	D_ASSERT(to_vector.auxiliary->GetBufferType() == VectorBufferType::GEOGRAPHY_BUFFER);

	auto &geography_buffer = (GeographyBuffer &)*to_vector.auxiliary;

	auto from_obj = (from_ptr + from_idx);
	auto coords = geography_buffer.AddPoints(from_obj->GetLngs(), from_obj->GetLats(), from_obj->NumPoints());

	auto obj_copy = new (to_ptr + to_idx) Geography(*from_obj);
	obj_copy->SetCoords(coords[0], coords[1]);
}

GeographyType GeographyVector::GetGeoType(const Vector &vector) {
	D_ASSERT(vector.GetType().id() == LogicalTypeId::GEOGRAPHY);

	if (vector.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		return FlatVector::GetData<Geography>(vector)->GetType();
	}

	if (vector.auxiliary) {
		D_ASSERT(vector.auxiliary->GetBufferType() == VectorBufferType::GEOGRAPHY_BUFFER);
		return ((GeographyBuffer &)*vector.auxiliary).GetGeoType();
	}
	return GeographyType::UNKNOWN;
}

bool GeographyVector::IsGeoType(const Vector &vector, const GeographyType geo_type) {
	return GetGeoType(vector) == geo_type;
}

Geography &GeographyVectorWriter::AddPoint(double lng, double lat) {
	return GeographyVector::AddPoint(vector, current_idx++, lng, lat);
}

Geography &GeographyVectorWriter::AddEmpty(GeographyType type) {
	return GeographyVector::AddEmpty(vector, current_idx++, type);
}

void GeographyVectorWriter::AddNull() {
	FlatVector::SetNull(vector, current_idx++, true);
}

} // namespace duckdb
