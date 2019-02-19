#!/bin/bash
## ======================================================================== ##
## Copyright 2015-2019 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

mkdir build
cd build

cmake \
  -D pxr_DIR=/gitlab/USD-install \
  -D ospray_DIR=/gitlab/ospray-install \
  -D PXR_BUILD_OPENIMAGEIO_PLUGIN=ON \
  -D OIIO_BASE_DIR=/gitlab/USD-install \
  -D HDOSPRAY_ENABLE_DENOISER=OFF \
  -D CMAKE_INSTALL_PREFIX=/gitlab/USD-install \
  "$@" ..

make -j`nproc`
