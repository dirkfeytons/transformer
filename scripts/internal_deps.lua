#!/usr/bin/env lua
--[[
Copyright (c) 2017 Technicolor Delivery Technologies, SAS

The source code form of this Transformer component is subject
to the terms of the Clear BSD license.

You can redistribute it and/or modify it under the terms of the
Clear BSD License (http://directory.fsf.org/wiki/License:ClearBSD)

See LICENSE file for more details.
]]

-- This script must be called from the root of the repo.
--   ./lua scripts/internal_deps.lua
-- The resulting deps.dot can be turned into an image with
--   dot -Tsvg -o deps.svg deps.dot

local lfs = require("lfs")
local concat = table.concat
local lines = io.lines
local pairs, type = pairs, type

local requires = {}
local file_tree = {}

local function extract_requires(full_path, full_name)
  local file_data = requires[full_name]
  if not file_data then
    file_data = {}
    requires[full_name] = file_data
  end
  for line in lines(full_path) do
    if not line:match("^%s*%-%-") then
      local package = line:match("require%s*%(?[\"'](transformer%..-[^%.]+)[\"']%)?")
      if package then
        package = package:gsub("%.", "_") .. "_lua"
        file_data[#file_data + 1] = package
      end
    end
  end
end

local function iterate(location, t, full_name)
  for entry in lfs.dir(location) do
    if entry ~= "." and entry ~= ".." then
      local full_path = location .. "/" .. entry
      local mode = lfs.attributes(full_path, "mode")
      if mode == "file" then
        --local module = entry:gsub("%.lua$", "")
        t[entry] = (full_name .. "_" .. entry):gsub("%.", "_")
        extract_requires(full_path, t[entry])
      elseif mode == "directory" then
        t[entry] = {}
        iterate(full_path, t[entry], full_name .. "_" .. entry)
      end
    end
  end
end
iterate("transformer", file_tree, "transformer")

-- generate a .dot file
local f = io.open("deps.dot", "w")
local cluster_count = 0

local function fwrite(fmt, ...)
  f:write(fmt:format(...), "\n")
end

local function render_tree(tree, nodename, indent)
  fwrite('%ssubgraph cluster_%d {', ("  "):rep(indent), cluster_count)
  indent = indent + 1
  fwrite('%slabel = "%s";', ("  "):rep(indent), nodename)
  f:write(("  "):rep(indent), 'style = "dashed";', "\n")
  cluster_count = cluster_count + 1
  for nodename, value in pairs(tree) do
    if type(value) == "string" then
      fwrite('%s%s [label = "%s"];', ("  "):rep(indent), value, nodename)
    else
      render_tree(value, nodename, indent)
    end
  end
  indent = indent - 1
  f:write(("  "):rep(indent), "}", "\n")
end

f:write("digraph {", "\n")
f:write("  // clusters and nodes", "\n")
render_tree(file_tree, "transformer", 1)
f:write("  // edges", "\n")
for module, requirelist in pairs(requires) do
  if #requirelist > 0 then
    fwrite("  %s -> {%s}", module, concat(requirelist, " "))
  end
end
f:write("}", "\n")
f:close()
