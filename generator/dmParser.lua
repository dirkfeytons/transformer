--[[
Copyright (c) 2016 Technicolor Delivery Technologies, SAS

The source code form of this Transformer component is subject
to the terms of the Clear BSD license.

You can redistribute it and/or modify it under the terms of the
Clear BSD License (http://directory.fsf.org/wiki/License:ClearBSD)

See LICENSE file for more details.
]]

local M = {}

local insert = table.insert

function M.parse(filename, path)

  local dm = {} -- the DM file will be parsed to this table
  local numEntries = {} -- info about the numEntries parameters found
  local xmlStack = {} -- a stack of open XML elements
  local object = nil -- the object that is currently parsed
  local parameter = nil  -- the parameter that is currently parsed
  local parameterName = nil -- the name of the parameter that is currently parsed
  local dataTypeName = nil -- the name of the dataType that is currently parsed
  local enums = {} -- a local table keeping track of all enumerations used.
  local adding = false -- a boolean tracking if we should add to the dm or not.
  local haveAlias = false

  -- A local table to translate types in the XML file to TR-069 SOAP types that
  -- will be expanded with the dataTypes defined within the XML file.
  local typeTable = {
    base64 = "base64",
    boolean = "boolean",
    dateTime = "dateTime",
    hexBinary = "hexBinary", -- warning: no corresponding TR-069 SOAP type!
    int = "int",
    long = "long", -- warning: no corresponding TR-069 SOAP type!
    string = "string",
    unsignedInt = "unsignedInt",
    unsignedLong = "unsignedLong" -- warning: no corresponding TR-069 SOAP type!
  }

  --- Adds an element to an ordered table. If v is a table and
  -- t[k] is also a table, the elements of v will be added to the
  -- t[k] table.
  local function add(t,k,v)
    if not t[k] then
      t[#t+1] = k
    end
    if t[k] and type(t[k])=='table' then
      if type(v) == 'table' then
        for i=1,#v do
          add(t[k],v[i],v[v[i]])
        end
      else
        print("overwriting a table")
        t[k] = v
      end
    else
      t[k] = v
    end
  end

  local function getTable(t,k)
    if not t[k] then
      add(t,k,{})
    end
    return t[k]
  end

  local function getParentObject(path)
    local parent
    if path:match("%.{i}%.$") then
      -- Multi instance object
      parent = path:gsub("[^%.]*%.{i}%.$", "")
    else
      -- Single instance object
      parent = path:gsub("[^%.]*%.$", "")
    end
    return parent
  end

  --- Translate an enumerationRef to an object type path.
  local function translateEnumerationRef(ref,path)
    local distantRef = ref:match("#")
    if distantRef then
      -- The reference is outside the current object.
      local refStripped, levels = ref:gsub("#","")
      local parentPath = path
      while levels > 0 do
        parentPath = getParentObject(parentPath)
        levels = levels - 1
      end
      -- Strip possible trailing dot
      parentPath = parentPath:gsub("%.$","")
      return parentPath..refStripped
    else
      return path..ref
    end
  end

  --- Translate a pathRef to an object type path.
  local function translatePathRef(ref,path,kind)
    local result = nil
    for m in ref:gmatch("[^%s]+") do
      local relativePath = m:match("#") or not m:match("^%.")
      local q = m
      if kind == "row" then
        q = m.."{i}."
      end
      if relativePath then
        if ref:match("^#%.") then
          -- Go up one level
          local parentPath = path:gsub("%.[^.]+[.{i}]*$","")
          q = parentPath..q:gsub("#","")
        elseif ref:match("^##%.") then
          -- Go up two levels
          local parentPath = path:gsub("%.[^.]+[.{i}]*$","")
          parentPath = parentPath:gsub("%.[^.]+[.{i}]*$","")
          q = parentPath..q:gsub("#","")
        elseif ref:match("^###%.") then
          -- Go up three levels
          local parentPath = path:gsub("%.[^.]+[.{i}]*$","")
          parentPath = parentPath:gsub("%.[^.]+[.{i}]*$","")
          parentPath = parentPath:gsub("%.[^.]+[.{i}]*$","")
          q = parentPath..q:gsub("#","")
        else
          -- same level, don't go up any levels.
          q = path..q
        end
      else
        local root = path:match("^[^.]+")
        if root then
          q = root..q
        end
      end
      if not result then
        result = q
      else
        result = result .." "..q
      end
    end
    return result
  end

  local callbacks = {
    StartElement = function(parser, name, attrs)
      -- add opened element to stack
      xmlStack[#xmlStack+1] = name

      --- Additional data type definitions
      -- Dataype element
      if xmlStack[#xmlStack-1] == "dm:document" and name == "dataType" then
        -- base attribute refers to a known base type
        if attrs["base"] and typeTable[attrs["base"]] then
          typeTable[attrs["name"]] = typeTable[attrs["base"]]
        end
        -- complex types contain datatype child elements
        dataTypeName = attrs["name"]
      end

      -- Children of datatype elements
      if xmlStack[#xmlStack-1] == "dataType" then
        if typeTable[name] then
          typeTable[dataTypeName] = typeTable[name]
        end
      end

      if xmlStack[#xmlStack-1] == "dm:document" and name == "model" then
        dm["name"] = attrs["name"]
      end

      --- Datamodel definitions
      -- Object element
      if xmlStack[#xmlStack-1] == "model" and name == "object" then
        local objectType = attrs['name']
        local numEntriesParameter = attrs['numEntriesParameter']
        if numEntriesParameter then
            local parent = objectType:match("(.*%.)[^.]*%.{i}%.")
            if parent then
                local info = numEntries[parent]
                if not info then
                    info = {}
                    numEntries[parent] = info
                end
                info[numEntriesParameter] = objectType
            end
        end
        object = attrs
        object["params"] = {}
        haveAlias = false
        if (not path or string.find(objectType, path)) then
          adding = true
          add(dm, objectType, object)
        end
      end

      -- Parameter element
      if xmlStack[#xmlStack-2] == "model" and name == "parameter" then
        if object then
          add(object["params"], attrs["name"], attrs)
          parameterName = attrs["name"]
          if parameterName == "Alias" then
            haveAlias = true
          end
          parameter = getTable(object["params"], parameterName)
        end
      end

      -- Syntax element
      if parameter and name == "syntax" then
        for i,v in ipairs(attrs) do
          add(parameter, attrs[i], attrs[attrs[i]])
        end
      end

      -- Child elements of syntax element
      if parameter and xmlStack[#xmlStack-1] == "syntax" then
        if name == "default" then
          add(parameter, "default", attrs["value"])
        end
        if name == "dataType" and typeTable[attrs["ref"]] then
          add(parameter, "type", typeTable[attrs["ref"]])
          if attrs["ref"]=="Alias" then
            if not object.aliasParameter then
              object[#object+1] = "aliasParameter"
              object.aliasParameter = parameterName
            end
          end
        end
        if name == "list" then
          add(parameter, "list", true)
          if attrs and #attrs > 0 then
            if attrs["maxItems"] then
              add(parameter, "maxItems", attrs["maxItems"])
            end
            if attrs["minItems"] then
              add(parameter, "minItems", attrs["minItems"])
            end
          end
        end
        if typeTable[name] then
          add(parameter, "type", typeTable[name])
        end
      end

      if parameter and xmlStack[#xmlStack-2] == "syntax" then
        if name == "size" and attrs and #attrs > 0 then
          -- Possible attributes are minLength, maxLength
          if attrs["minLength"] then
            add(parameter, "min", attrs["minLength"])
          end
          if attrs["maxLength"] then
            add(parameter, "max", attrs["maxLength"])
          end
        end
        if name == "range" and attrs and #attrs > 0 then
          -- Possible attributes are minInclusive, maxInclusive
          -- Multiple ranges are possible, eg. int­[-1:15, 16:31]
          local range = getTable(parameter, "range")
          local entry = {}
          if attrs["minInclusive"] then
            add(entry, "min", attrs["minInclusive"])
          end
          if attrs["maxInclusive"] then
            add(entry, "max", attrs["maxInclusive"])
          end
          if #entry > 0 then
            insert(range,entry)
          end
        end
        if name == "enumeration" and attrs and #attrs > 0 then
          -- Should only occur in string type.
          -- Possible attributes are value, optional, status, access
          if attrs["value"] and (attrs["status"] == nil or attrs["status"] ~= "deprecated") then
            local enum = enums[object["name"]..parameter["name"]]
            if not enum then
              enum = {}
              enums[object["name"]..parameter["name"]] = enum
            end
            insert(enum,attrs["value"])
            if not parameter["enumeration"] then
              add(parameter, "enumeration", enum)
            end
          end
        end
        if name == "enumerationRef" and attrs and #attrs > 0 then
          -- Should only occur in string type.
          -- Possible attributes are targetParam and nullValue
          if attrs["targetParam"] then
            local refPath = translateEnumerationRef(attrs["targetParam"],object["name"])
            local enum = enums[refPath]
            if not enum then
              enum = {}
              enums[refPath] = enum
            end
            if attrs["nullValue"] then
              insert(enum,attrs["nullValue"])
            end
            if not parameter["enumeration"] then
              add(parameter, "enumeration", enum)
            end
          end
        end
        if name == "pathRef" and attrs and #attrs >0 then
          add(parameter, "pathRef", true)
          -- Possible attributes are refType, targetParent and targetType
          if attrs["targetParent"] then
            local pathRef = translatePathRef(attrs["targetParent"],object["name"],attrs["targetType"])
            add(parameter, "targetParent", pathRef)
          end
        end
        --- Enable this if you wish to have a units field in your mappings.
        --  if name == "units" and attrs and #attrs > 0 then
        --    -- Only possible attribute is value
        --    if attrs["value"] then
        --      add(parameter, "units", attrs["value"])
        --    end
        --  end
        --- Enable this if you wish to have a pattern field in your mappings.
        --  if name == "pattern" and attrs and #attrs > 0 then
        --    -- Only possible attribute is value
        --    if attrs["value"] then
        --      table.insert(getTable(parameter,"pattern"),attrs["value"])
        --    end
        --  end
      end
    end,

    EndElement = function(parser, name)
      -- remove closed element from stack
      xmlStack[#xmlStack] = nil

      if name == "object" and object then
        -- remove pointer to object
        if not object.aliasParameter and haveAlias then
          object[#object+1] = "aliasParameter"
          object.aliasParameter = "Alias"
        end
        object = nil
        adding = false
      end

      if name == "parameter" and parameter then
        assert(parameter.type, "could not determine type of " .. parameter.name)
        -- remove pointer to parameter
        parameter = nil
        parameterName = nil
      end

      if name == "datatype" and dataTypeName then
        -- remove pointer to datatype
        dataTypeName = nil
      end
    end
  }

  -- Parse the XML datamodel using lxp
  local parser = require('lxp').new(callbacks)
  local fileDescriptor = io.open(filename)
  local dmXML = fileDescriptor:read("*all")
  parser:parse(dmXML)
  parser:close()
  fileDescriptor:close()

  return dm, numEntries
end

return M
