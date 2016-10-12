#!/bin/sh
# Copyright (c) 2016 Technicolor Delivery Technologies, SAS
#
# The source code form of this Transformer component is subject
# to the terms of the Clear BSD license.
#
# You can redistribute it and/or modify it under the terms of the
# Clear BSD License (http://directory.fsf.org/wiki/License:ClearBSD)
#
# See LICENSE file for more details.

gen_dir=$(dirname $(readlink -f $0))
export LUA_CPATH=${gen_dir}/lib/$(uname -m)/?.so
export LUA_PATH="${gen_dir}/?.lua;${gen_dir}/lib/?.lua;${gen_dir}/lib/pl/?.lua"
lua ${gen_dir}/generator.lua "$@"
