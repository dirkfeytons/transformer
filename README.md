# Transformer

## Description

`Transformer` is an application that provides a datamodel mapping framework.
More specifically it allows one to implement a Broadband Forum [TR-106](https://www.broadband-forum.org/cwmp.php)
based datamodel without having to worry too much about the requirements of TR-106
and instead focus on the logic to map that datamodel to the system's management
plane.

Documentation on how a mapping file must look like can be found in `./doc/skeleton.map`.
The datamodel metadata part of a mapping file can be generated using `./generator/generator.sh`.

## Dependencies

`Transformer` requires the following dependencies to work:

- [Lua](http://www.lua.org)
- [LuaFileSystem](https://github.com/keplerproject/luafilesystem)
- [Lua BitOp](http://bitop.luajit.org/)
- [SQLite3 and LuaSQLite3](http://lua.sqlite.org/index.cgi/index)
- [lua-tch](https://github.com/dirkfeytons/lua-tch)
- [UCI](https://git.lede-project.org/?p=project/uci.git)
- [ubus](https://git.lede-project.org/?p=project/ubus.git) (not required for
  starting up; only if mappings use the ubus eventsource)

## License

`Transformer` mainly uses the 2-clause ClearBSD license. There are some
third party open source software components included with their own license.

For detailse see the `LICENSE` file.
