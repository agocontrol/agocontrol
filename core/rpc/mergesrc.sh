#!/usr/bin/env bash

OUTFILE=$1
shift

rm -f $OUTFILE

mkdir -p $(dirname $OUTFILE)
touch $OUTFILE
for INFILE in $*; do
    echo "Adding into $OUTFILE: $(basename $INFILE)"
    echo "/* BEGIN $(basename $INFILE) */" >> $OUTFILE
    cat $INFILE >> $OUTFILE
    echo "/* END $(basename $INFILE) */" >> $OUTFILE
done
