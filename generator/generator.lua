--[[
Copyright (c) 2016 Technicolor Delivery Technologies, SAS

The source code form of this Transformer component is subject
to the terms of the Clear BSD license.

You can redistribute it and/or modify it under the terms of the
Clear BSD License (http://directory.fsf.org/wiki/License:ClearBSD)

See LICENSE file for more details.
]]

-- Change this if you change the output of the generator!
local version = "2.3"

local function printHeader(out, datamodel)
  out:write("-- Automatically generated from ", datamodel, "\n",
            "-- using generator version ", version, "\n")
end

--- Prints a mapping for a datamodel to an output stream.
-- @param out The output stream to which the datamodel will be written.
-- @param datamodel The datamodel for which a mapping will be written.
-- @return nil
local function printMapping(out, datamodel, numEntriesInfo)

  local printOrderedTable

  local function printUnOrderedTable(out, table, indent, conversionTable)
    local indent = indent or ''
    local conversion = {}
    for _, value in pairs(table) do
      conversion = conversionTable[value]
      if not (conversion == false) then
        if type(value) == 'table' and #value > 0 and not value[value[1]] then
          out:write(indent,'{\n')
          printUnOrderedTable(out, value, indent..'  ', conversionTable)
          out:write(indent, '}')
        elseif type(value) == 'table' then
          out:write(indent,'{\n')
          printOrderedTable(out, value, indent..'  ', conversionTable)
          out:write(indent, '}')
        elseif type(conversion) == 'function' then
          out:write(indent,conversion(value))
        elseif type(value) == 'boolean' then
          out:write(indent,tostring(value))
        else
          out:write(indent,'"', value, '"')
        end
        out:write(',\n')
      end
    end
  end

  --- Prints a table in the order specified by the #table first elements.
  -- @param out The output stream to which the table will be written.
  -- @param table The datamodel for which a mapping will be created.
  -- @param indent The indentation that will be printed before each table entry.
  -- @param conversionTable A table that specifies how certain keys should be converted.
  --   Keys can be ignored if the conversionTable for it returns false.
  -- @return nil
  printOrderedTable = function(out, table, indent, conversionTable)
    local indent = indent or ''
    conversion = conversion or {}
    for i=1,#table do
      local key = table[i]
      local value = table[key] or table[i]
      conversion = conversionTable[key]
      if not (conversion == false) then
        out:write(indent, key, ' = ')
        if type(value) == 'table' and #value > 0 and not value[value[1]] then
          out:write('{\n')
          printUnOrderedTable(out, value, indent..'  ', conversionTable)
          out:write(indent, '}')
        elseif type(value) == 'table' then
          out:write('{\n')
          printOrderedTable(out, value, indent..'  ', conversionTable)
          out:write(indent, '}')
        elseif type(conversion) == 'function' then
          out:write(conversion(value))
        elseif type(value) == 'boolean' then
          out:write(tostring(value))
        else
          out:write('"', value, '"')
        end
        out:write(',\n')
      end
    end
  end

  -- A conversion function for Min and MaxEntries.
  local function entriesConversion(value)
    if value == "unbounded" then
      return "math.huge"
    end
    return value
  end

  -- Loop over the datamodel and write it to out stream.
  for objectIndex=1,#datamodel do
    local objectName = datamodel[objectIndex]
    local objectAttributes = datamodel[objectName]
    -- Converts and collapses series of non-alphanumeric characters to underscores.
    local objectNameVar = string.gsub(objectName, "%W+", "_")
    out:write('local ', objectNameVar, ' = {\n')
    out:write('  objectType = {\n')
    printOrderedTable(out, objectAttributes, '    ',
      { ["dmr:version"] = false, ["dmr:noUniqueKeys"] = false, ["dmr:fixedObject"] = false,
        maxEntries = entriesConversion, minEntries = entriesConversion  })
    out:write('    parameters = {\n')
    local numEntries = numEntriesInfo[objectName] or {}
    for parameterIndex=1,#objectAttributes.params do
      local parameterName = objectAttributes.params[parameterIndex]
      if not numEntries[parameterName] then
        local parameterAttributes = objectAttributes.params[parameterName]
        out:write('      ', parameterName, ' = {\n')
        printOrderedTable(out, parameterAttributes, '        ',
          { name = false, ["dmr:version"] = false })
        out:write('      },\n')
      else
        out:write("      -- ", parameterName, "\n",
                  "      -- automatically created when ",
                  numEntries[parameterName], " is loaded\n")
      end
    end
    out:write('    }\n')
    out:write('  }\n')
    out:write('}\n\n')
    out:write('register(', objectNameVar, ')\n\n')
  end

end

function listObjectTypes(out, datamodel)
    local allNames = {}
    for idx=1,#datamodel do
        allNames[#allNames+1] = datamodel[idx]
    end
    table.sort(allNames)
    for idx=1,#allNames do
        out:write(allNames[idx],"\n")
    end
end

-- Main program

local args = require('lapp') ([[
Generates a mapping file for Transformer (version ]] ..version.. [[).
  -o, --output (default stdout) The path to which the mapping file will be written.
  -d, --datamodel (default ']] .. string.gsub(arg[0],'/[^/]*$',"") .. [[/dm/Dev-2.11.xml') The path of the BBF XML description on which the mapping should be created.
  <path> (string) The BBF path for which a datamodel should be created. ('?' to list them)
]])

local list = false
local path=args.path
if path=='?' then
    path = nil
    list = true
else
    path = "^"..path.."$"
end

local dataModel, numEntries = require('dmParser').parse(args.datamodel, path)

if not list then
    printHeader(args.output, dataModel["name"])
    dataModel["name"] = nil
    printMapping(args.output, dataModel, numEntries)
else
    listObjectTypes(args.output, dataModel)
end
