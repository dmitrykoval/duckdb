import os
import sys
import duckdb
import pandas as pd
import pyarrow as pa
import time

threads = None
verbose = False
out_file = None
nruns = 10
TPCH_NQUERIES = 22

for arg in sys.argv:
    if arg == "--verbose":
        verbose = True
    elif arg.startswith("--threads="):
        threads = int(arg.replace("--threads=", ""))
    elif arg.startswith("--nruns="):
        nruns = int(arg.replace("--nruns=", ""))
    elif arg.startswith("--out-file="):
        out_file = arg.replace("--out-file=", "")

# generate data
if verbose:
    print("Generating TPC-H data")

main_con = duckdb.connect()
main_con.execute('CALL dbgen(sf=1)')

tables = [
 "customer",
 "lineitem",
 "nation",
 "orders",
 "part",
 "partsupp",
 "region",
 "supplier",
]

def open_connection():
    con = duckdb.connect()
    if threads is not None:
        if verbose:
            print(f'Limiting threads to {threads}')
        con.execute(f"SET threads={threads}")
    return con

def benchmark_query(benchmark_name, con, query):
    if verbose:
        print(benchmark_name)
        print(query)
    for nrun in range(nruns):
        start = time.time()
        df_result = con.execute(query).df()
        end = time.time()

        bench_result = f"{benchmark_name}\t{nrun}\t{end - start}"

        if out_file is not None:
            f.write(bench_result)
            f.write('\n')
        else:
            print(bench_result)

def run_tpch(con, prefix):
    for i in range(1, TPCH_NQUERIES + 1):
        benchmark_name = "%stpch_q%02d" % (prefix, i)
        benchmark_query(benchmark_name, con, f'PRAGMA tpch({i})')


if out_file is not None:
    f = open(out_file, 'w+')

# pandas scans
data_frames = {}
for table in tables:
    data_frames[table] = main_con.execute(f"SELECT * FROM {table}").df()

df_con = open_connection()
for table in tables:
    df_con.register(table, data_frames[table])

run_tpch(df_con, "pandas_")

# # arrow scans
# arrow_tables = {}
# for table in tables:
#     arrow_tables[table] = pa.Table.from_pandas(data_frames[table])
#
# arrow_con = open_connection()
# for table in tables:
#     arrow_con.register(table, arrow_tables[table])
#
# run_tpch(arrow_con, "arrow_")
