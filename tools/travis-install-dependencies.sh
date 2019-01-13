#!/bin/sh
# Installs dependencies of preCICE
# boost, Eigen and PETSc
# if not already cached by Travis.

set -x
set -e

LOCAL_INSTALL=$1


# Don't test for $LOCAL_INSTALL, because it's created by the cacher.
if [ ! -d $LOCAL_INSTALL/include ]; then
    mkdir -p $LOCAL_INSTALL/include $LOCAL_INSTALL/lib

    # Download and extract Eigen with cmake
    mkdir -p $LOCAL_INSTALL/eigen3
    wget -nv http://bitbucket.org/eigen/eigen/get/3.3.2.tar.bz2 -O - | tar xj -C $LOCAL_INSTALL/eigen3 --strip-components=1 eigen-eigen-da9b4e14c255

    wget -nv 'http://downloads.sourceforge.net/project/boost/boost/1.60.0/boost_1_60_0.tar.bz2' -O - | tar xj
    cd boost_1_60_0
    ./bootstrap.sh --prefix=$LOCAL_INSTALL > ~/boost.bootstrap
    ./b2 -j2 --with-program_options --with-test --with-filesystem --with-log install > ~/boost.b2

    # Download and compile PETSc
    cd $LOCAL_INSTALL
    git clone -b maint https://bitbucket.org/petsc/petsc petsc
    cd petsc
    python2 configure --with-debugging=1
    make > ~/petsc.make
fi

# get version of cmake, that works with boost 1.60.0
#if [ ! -d $LOCAL_INSTALL/cmake ]; then
#  mkdir -p ${LOCAL_INSTALL}/cmake
#  if [ "$TRAVIS_OS_NAME" = "osx" ]; then
#    CMAKE_URL="https://cmake.org/files/v3.10/cmake-3.10.1-Darwin-x86_64.tar.gz"
#  else
#    CMAKE_URL="http://www.cmake.org/files/v3.10/cmake-3.10.1-Linux-x86_64.tar.gz"
#  fi
#  mkdir -p ${LOCAL_INSTALL}/cmake
#  wget --no-check-certificate --quiet -O - ${CMAKE_URL} | tar --strip-components=1 -xz -C ${LOCAL_INSTALL}/cmake
#  # make cmake available
#fi
