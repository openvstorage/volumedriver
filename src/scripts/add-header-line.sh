#! /bin/bash
set -eux
for i in *.cpp
do
mv $i $i.old
echo "#include <$1.h>" > $i
cat $i.old >> $i
rm $i.old
done
