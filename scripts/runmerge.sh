#!/bin/bash

set -e -x

if test $# -lt 2
then
  echo "Usage: $0 <file1> <file2> ..."
  exit 1
fi

PREFIX=../testdata

../bin/gt suffixerator -indexname ${PREFIX}/all -db $* -suf -lcp -pl 1
num=0
indexlist=""
for filename in $*
do
  ../bin/gt suffixerator -indexname ${PREFIX}/midx${num} -db ${filename} -suf -lcp -tis -pl 1
  indexlist="${indexlist} ${PREFIX}/midx${num}"
  num=`expr ${num} + 1`
done
../bin/gt dev mergeesa -indexname ${PREFIX}/midx-all -ii ${indexlist}
cmp ${PREFIX}/midx-all.suf ${PREFIX}/all.suf
cmp ${PREFIX}/midx-all.lcp ${PREFIX}/all.lcp
cmp ${PREFIX}/midx-all.llv ${PREFIX}/all.llv
../bin/gt mkfmindex -fmout ${PREFIX}/fm-all -ii ${indexlist}
../bin/gt suffixerator -dna -tis -pl 1 -db ${PREFIX}/fm-all.bwt
