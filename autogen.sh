#!/bin/sh

mkdir -p m4
if [ $? != 0 ]; then
	echo "ERROR: Could not build necessary support directory 'm4'"
	exit 1
fi

if hash glibtoolize 2>/dev/null; then
	glibtoolize -i
else
	libtoolize -i
fi
if [ $? != 0 ]; then
	echo "ERROR: there was an error while libtoolizing qof."
	exit 2
fi

autoreconf -i 
if [ $? != 0 ]; then
	echo "ERROR: something went wrong while creating configure script."
	exit 3
fi
