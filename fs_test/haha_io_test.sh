#!/bin/bash

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <fs path>"
  exit 1
fi

fs=$1

echo "testing hahafs with fs path '$fs'"
echo "writing random numbers into files..."
for file in `ls $fs`
do
	if [ -f $fs/$file ];then
		num = $RANDOM
		echo "writing $num into $fs/$file"
		echo "$RANDOM" > $fs/$file
	fi
done

echo "reading numbers from files..."
for file in `ls $fs`
do
	if [ -f $fs/$file ];then
		echo "reading $fs/$file:"
		cat $fs/$file
	fi
done
