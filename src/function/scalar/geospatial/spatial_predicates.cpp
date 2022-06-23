#pragma once

#include "duckdb/common/types/geography_vector.hpp"
#include "duckdb/function/scalar/geospatial_functions.hpp"
#include "duckdb/common/spatial/WKTReader.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "s2/s2contains_point_query.h"
#include "s2/s2latlng.h"

namespace duckdb {

bool static AnyNullVector(const Vector &left, const Vector &right) {
	return ((left.GetVectorType() == VectorType::CONSTANT_VECTOR && ConstantVector::IsNull(left)) ||
	        (right.GetVectorType() == VectorType::CONSTANT_VECTOR && ConstantVector::IsNull(right)));
}

class SpatialPredicates {
public:
	static inline void Contains(Vector &outer_v, Vector &inner_v, Vector &result, idx_t nrows) {
		if (GeographyVector::IsGeoType(outer_v, GeographyType::POLYGON) &&
		    GeographyVector::IsGeoType(inner_v, GeographyType::POINT)) {

			ApplyBinaryPredicate<GeographyType::POLYGON, S2Polygon, GeographyType::POINT, S2Point>(
			    outer_v, inner_v, result, nrows,
			    [](const S2Polygon &outer, const S2Point &inner) { return outer.Contains(inner); });
		} else if (GeographyVector::IsGeoType(outer_v, GeographyType::POLYGON) &&
		           GeographyVector::IsGeoType(inner_v, GeographyType::LINESTRING)) {

			ApplyBinaryPredicate<GeographyType::POLYGON, S2Polygon, GeographyType::LINESTRING, S2Polyline>(
			    outer_v, inner_v, result, nrows,
			    [](const S2Polygon &outer, const S2Polyline &inner) { return outer.Contains(inner); });
		} else if (GeographyVector::IsGeoType(outer_v, GeographyType::LINESTRING) &&
		           GeographyVector::IsGeoType(inner_v, GeographyType::POINT)) {

			ApplyBinaryPredicate<GeographyType::LINESTRING, S2Polyline, GeographyType::POINT, S2Point>(
			    outer_v, inner_v, result, nrows,
			    [](const S2Polyline &outer, const S2Point &inner) { return outer.Contains(inner); });
		} else if (GeographyVector::IsGeoType(outer_v, GeographyType::MULTIPOLYGON) &&
		           GeographyVector::IsGeoType(inner_v, GeographyType::POINT)) {

			ApplyBinaryPredicate<GeographyType::MULTIPOLYGON, std::vector<S2Polygon>, GeographyType::POINT, S2Point>(
			    outer_v, inner_v, result, nrows,
			    [](const std::vector<S2Polygon> &outer, const S2Point &inner) {
				    for (auto &polygon : outer) {
					    if (polygon.Contains(inner)) {
						    return true;
					    }
				    }
				    return false;
			    });
		} else {
			throw InternalException(
			    "Containment of " + Geography::GeographyTypeToString(GeographyVector::GetGeoType(inner_v)) + " in " +
			    Geography::GeographyTypeToString(GeographyVector::GetGeoType(outer_v)) + " is not supported.");
		}
	}

private:
	template <GeographyType GEO_T_OUTER, typename S2_OUTER, GeographyType GEO_T_INNER, typename S2_INNER>
	static void ApplyBinaryPredicate(Vector &left_v, Vector &right_v, Vector &result, idx_t nrows,
	                                 std::function<bool(const S2_OUTER &, const S2_INNER &)> predicate) {
		auto left_it = GeographyVector::GetS2ObjectsIterator<GEO_T_OUTER, S2_OUTER>(left_v);
		auto right_it = GeographyVector::GetS2ObjectsIterator<GEO_T_INNER, S2_INNER>(right_v);

		auto res_data = result.GetData();

		VectorData vdata_left;
		VectorData vdata_right;
		left_v.Orrify(nrows, vdata_left);
		right_v.Orrify(nrows, vdata_right);

		if (left_v.GetVectorType() == VectorType::CONSTANT_VECTOR &&
		    right_v.GetVectorType() == VectorType::CONSTANT_VECTOR) {
			if (ConstantVector::IsNull(left_v) || ConstantVector::IsNull(right_v)) {
				ConstantVector::SetNull(result, true);
				return;
			}

			auto const left_obj = *left_it;
			auto const right_obj = *right_it;
			res_data[0] = predicate(left_obj, right_obj);
		} else if (left_v.GetVectorType() == VectorType::CONSTANT_VECTOR &&
		           right_v.GetVectorType() != VectorType::CONSTANT_VECTOR) {
			if (ConstantVector::IsNull(left_v)) {
				ConstantVector::SetNull(result, true);
				return;
			}

			auto left_obj = *left_it;
			for (size_t i = 0; i < nrows; i++, right_it++) {
				if (!vdata_right.validity.RowIsValid(i)) {
					FlatVector::SetNull(result, i, true);
					continue;
				}

				res_data[i] = predicate(left_obj, *right_it);
			}
		} else if (left_v.GetVectorType() != VectorType::CONSTANT_VECTOR &&
		           right_v.GetVectorType() == VectorType::CONSTANT_VECTOR) {
			if (ConstantVector::IsNull(right_v)) {
				ConstantVector::SetNull(result, true);
				return;
			}

			const auto &right_obj = *right_it;
			for (size_t i = 0; i < nrows; i++, left_it++) {
				if (!vdata_left.validity.RowIsValid(i)) {
					FlatVector::SetNull(result, i, true);
					continue;
				}

				res_data[i] = predicate(*left_it, right_obj);
			}
		} else {
			for (size_t i = 0; i < nrows; i++, left_it++, right_it++) {
				if (!vdata_left.validity.RowIsValid(i) || !vdata_right.validity.RowIsValid(i)) {
					FlatVector::SetNull(result, i, true);
					continue;
				}

				res_data[i] = predicate(*left_it, *right_it);
			}
		}
	}
};


//===--------------------------------------------------------------------===//
// ST_FROM_WKT
//===--------------------------------------------------------------------===//
static void FromWKTFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 1);
	input.Normalify();

	GeographyVectorWriter writer(result);
	WKTReader wkt_reader(writer);

	auto &input_col = input.data[0];
	if (input_col.GetVectorType() == VectorType::CONSTANT_VECTOR) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
		if (ConstantVector::IsNull(input_col)) {
			// constant null, skip
			ConstantVector::SetNull(result, true);
			return;
		}
		auto input_data = ConstantVector::GetData<string_t>(input_col);
		wkt_reader.Read(input_data->GetString());
	} else {
		//		 non-constant vector: set the result type to a flat vector
		result.SetVectorType(VectorType::FLAT_VECTOR);
		// now get the lengths of each of the input elements
		VectorData vdata;
		input_col.Orrify(input.size(), vdata);

		auto input_data = (string_t *)vdata.data;
		// now add the length of each vector to the result length
		for (idx_t i = 0; i < input.size(); i++) {
			auto idx = vdata.sel->get_index(i);
			if (!vdata.validity.RowIsValid(idx)) {
				writer.AddNull();
				std::cout << "FLAT_VECTOR null. idx: " << i << std::endl;
				continue;
			}
			wkt_reader.Read(input_data[idx].GetString());
		}
	}
}

//===--------------------------------------------------------------------===//
// ST_MAKE_POINT
//===--------------------------------------------------------------------===//
static void MakePointFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 2);
	absl::SetFlag(&FLAGS_s2debug, false);

	auto nrows = input.size();

	auto &lng_col = input.data[0];
	auto &lat_col = input.data[1];

	if (AnyNullVector(lng_col, lat_col)) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
		ConstantVector::SetNull(result, true);
		return ;
	}

	input.Normalify();

	VectorData vdata_lng;
	VectorData vdata_lat;
	lng_col.Orrify(nrows, vdata_lng);
	lat_col.Orrify(nrows, vdata_lat);
	auto lng_input = (double *)vdata_lng.data;
	auto lat_input = (double *)vdata_lat.data;

	GeographyVectorWriter writer(result);

	for (idx_t i = 0; i < nrows; i++) {
		if (!vdata_lng.validity.RowIsValid(i) || !vdata_lat.validity.RowIsValid(i)) {
			writer.AddNull();
			continue;
		}

		writer.AddPoint(lng_input[i], lat_input[i]);
	}
}

//===--------------------------------------------------------------------===//
// ST_CONTAINS
//===--------------------------------------------------------------------===//
static void ContainsFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 2);

	auto const nrows = input.size();
	auto &left_arg = input.data[0];
	auto &right_arg = input.data[1];

	if (AnyNullVector(left_arg, right_arg)) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
		ConstantVector::SetNull(result, true);
		return ;
	}

	input.Normalify();

	SpatialPredicates::Contains(left_arg, right_arg, result, nrows);
}

//===--------------------------------------------------------------------===//
// ST_WITHIN
//===--------------------------------------------------------------------===//
static void WithinFunction(DataChunk &input, ExpressionState &state, Vector &result) {
	D_ASSERT(input.ColumnCount() == 2);

	auto const nrows = input.size();
	auto &left_arg = input.data[0];
	auto &right_arg = input.data[1];

	if (AnyNullVector(left_arg, right_arg)) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
		ConstantVector::SetNull(result, true);
		return ;
	}

	input.Normalify();

	SpatialPredicates::Contains(right_arg, left_arg, result, nrows);
}

void GeoFromWKTFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(ScalarFunction("st_from_wkt", {LogicalType::VARCHAR}, LogicalType::GEOGRAPHY, FromWKTFunction));
}

void GeoMakePointFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(ScalarFunction("st_make_point", {LogicalType::DOUBLE, LogicalType::DOUBLE}, LogicalType::GEOGRAPHY,
	                               MakePointFunction));
}

void GeoContainsFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(ScalarFunction("st_contains", {LogicalType::GEOGRAPHY, LogicalType::GEOGRAPHY}, LogicalType::BOOLEAN,
	                               ContainsFunction));
}

void GeoWithinFun::RegisterFunction(BuiltinFunctions &set) {
	set.AddFunction(ScalarFunction("st_within", {LogicalType::GEOGRAPHY, LogicalType::GEOGRAPHY}, LogicalType::BOOLEAN,
	                               WithinFunction));
}

} // namespace duckdb
