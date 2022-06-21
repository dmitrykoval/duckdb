# DuckDB Geography type memory layout proposal

This proposal focuses on memory layout of new logical Geography type for DuckDB.

### Key Design considerations

- Geography type assumes representation of angular coordinates, latitude and longitude in [WGS84](https://en.wikipedia.org/wiki/World_Geodetic_System) coordinate system. However, memory layout is totally applicable to planar x,y coordinates. 
- Latitude and Longitude are stored as two separate columns facilitating compression, to make use of spatial locality of adjacent data points. ie GPS coordinates are usually spatially close to each other, points in polygon co close to each other. Delta encoding is usually an effective choice for coordinates compression.
- All the coordinates for given vector are stored as contiguous arrays of doubles. Each Geography record appends its coordinates to arrays of latitude and longitude and saves offset and lengths.
- Supported OGC [Simple Feature Access](https://www.ogc.org/standards/sfa) types:
  - Point
  - LineString
  - Polygon
  - MultiPoint
  - MultiLineString
  - MultiPolygon
  - GeometryCollection
- Optional S2CellId based index. This would allow to push down some spatial predicates to the extended Scan operator and skip records, which don't satisfy the criteria.


### In-Memory Data layout

Logical View:

| Logical Record ID | latitude vector<double_t> | longitude vector<double_t> | type vector<uint8_t>  enums | lines_len vector<size_t>  "How many points?" | multi_len vector<size_t>  "How many lines?" | coll_mpolly_len vector<size_t>  "How many polygons?" | s2cellid vector<uint64_t>  optional index |
|-------------------|---------------------------|----------------------------|-----------------------------|----------------------------------------------|---------------------------------------------|------------------------------------------------------|-------------------------------------------|
| 0                 | 52.5255                   | 13.3463                    | Point                       | 1                                            |                                             |                                                      | 824687234                                 |
| 1                 | 52.5255                   | 13.3463                    | LineString                  | 3                                            |                                             |                                                      | 824687234                                 |
|                   | 52.5180                   | 13.3496                    |                             |                                              |                                             |                                                      |                                           |
|                   | 52.5070                   | 13.3496                    |                             |                                              |                                             |                                                      |                                           |
| 2                 | 52.5255                   | 13.3469                    | Polygon                     | 4                                            | 2                                           |                                                      | 824687235                                 |
|                   | 52.5255                   | 13.3467                    |                             |                                              |                                             |                                                      |                                           |
|                   | 52.5180                   | 13.3468                    |                             |                                              |                                             |                                                      |                                           |
|                   | 52.5245                   | 13.3496                    |                             |                                              |                                             |                                                      |                                           |
|                   | 52.5246                   | 13.3496                    |                             | 3                                            |                                             |                                                      |                                           |
|                   | 52.5983                   | 13.2893                    |                             |                                              |                                             |                                                      |                                           |
|                   | 52.0245                   | 13.9856                    |                             |                                              |                                             |                                                      |                                           |


Physical Memory representation in DuckDB:

| latitude Vector <double_t> | longitude Vector <double_t> | ... | Vector\<Struct\>                                                 | ...                                   | ...                                                 | ...                                                 | ...                                                         | ...                                                 |
|----------------------------|-----------------------------|-----|------------------------------------------------------------------|---------------------------------------|-----------------------------------------------------|-----------------------------------------------------|-------------------------------------------------------------|-----------------------------------------------------|
|                            |                             |     | {'coord_offset': Vector<idx_t> }<br/> offset into coords vectors | {'type': Vector<uint8_t> }<br/> enums | {'lines_len': List<idx_t> }<br/> "How many points?" | {'multi_len': <List<idx_t> }<br/> "How many lines?" | {'coll_mpolly_len': List<idx_t> }<br/> "How many polygons?" | {'s2cellid': Vector<uint64_t> }<br/> optional index |
| 52.5255                    | 13.3463                     |     | 0                                                                | Point                                 | 1                                                   |                                                     |                                                             | 824687234                                           |
| 52.5255                    | 13.3463                     |     | 1                                                                | LineString                            | 3                                                   |                                                     |                                                             | 824687234                                           |
| 52.5180                    | 13.3496                     |     |                                                                  |                                       |                                                     |                                                     |                                                             |                                                     |
| 52.5070                    | 13.3496                     |     |                                                                  |                                       |                                                     |                                                     |                                                             |                                                     |
| 52.5255                    | 13.3469                     |     | 4                                                                | Polygon                               | 4                                                   | 2                                                   |                                                             | 824687235                                           |
| 52.5255                    | 13.3467                     |     |                                                                  |                                       |                                                     |                                                     |                                                             |                                                     |
| 52.5180                    | 13.3468                     |     |                                                                  |                                       |                                                     |                                                     |                                                             |                                                     |
| 52.5245                    | 13.3496                     |     |                                                                  |                                       |                                                     |                                                     |                                                             |                                                     |
| 52.5246                    | 13.3496                     |     |                                                                  |                                       | 3                                                   |                                                     |                                                             |                                                     |
| 52.5983                    | 13.2893                     |     |                                                                  |                                       |                                                     |                                                     |                                                             |                                                     |
| 52.0245                    | 13.9856                     |     |                                                                  |                                       |                                                     |                                                     |                                                             |                                                     |

Reference implementation in pure C++:
[geography_type](https://github.com/dmitrykoval/duckdb/blob/dkoval.spatial/src/include/duckdb/common/types/geography_type.hpp)

Implementation using Vector<STRUCT> physical DuckDB type:
TBD