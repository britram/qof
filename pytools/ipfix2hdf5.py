import ipfix
import qof
import pandas as pd

import argparse
import bz2
from sys import stdin, stdout, stderr

args = None

def parse_args():
    global args
    parser = argparse.ArgumentParser(description="Convert an IPFIX file or stream to HDF5")
    parser.add_argument('ienames', metavar="ie", nargs="+",
                        help="column(s) by IE name")
    parser.add_argument('--spec', '-s', metavar="specfile", action="append",
                        help="file to load additional IESpecs from")
    parser.add_argument('--file', '-f', metavar="file",
                        help="IPFIX file to read (default stdin)")
    parser.add_argument('--bzip2', '-j', action="store_const", const=True,
                        help="Decompress bz2-compressed IPFIX file")
    parser.add_argument('--hdf5', '-5', metavar="file",
                        help="HDF5 store to use")
    parser.add_argument('--table', '-t', metavar="name",
                        help="table in HDF5 store")
    args = parser.parse_args()

def init_ipfix(specfiles = None):
    ipfix.ie.use_iana_default()
    ipfix.ie.use_5103_default()
    
    if specfiles:
        for sf in specfiles:
            ipfix.ie.use_specfile(sf)

def stream_to_hdf5(ipfix_stream, ienames, hdf5_store, hdf5_table):
    store = pd.HDFStore(hdf5_store)
    df = qof.dataframe_from_ipfix_stream(ipfix_stream, ienames)
    qof.coerce_timestamps(df)
    store[hdf5_table] = df
    store.close()

parse_args()
init_ipfix(args.spec)

if args.file:
    if args.bzip2:
        with bz2.open (args.file, mode="rb") as f:
            stream_to_hdf5(f, args.ienames, args.hdf5, args.table)
    else:
        with open (args.file, mode="rb") as f:
            stream_to_hdf5(f, args.ienames, args.hdf5, args.table)
else:
    stdin = stdin.detach()
    stream_to_hdf5(stdin, args.ienames, args.hdf5, args.table)