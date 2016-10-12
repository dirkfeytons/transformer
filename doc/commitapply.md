Commit & Apply for Transformer
==============================

Description
-----------
Transformer can automatically execute the necessary actions to apply
configuration changes. Typical example is restarting some daemons
after their UCI configuration was updated.

To accomplish this Transformer needs some information on how changes
to the datamodel relate to which action to execute to apply these changes.
This information must be provided in so-called rule files that are read on
startup (similar to the mapping files).

On every set, add or delete on the datamodel the mapping code or the
mapper has to inform the commit & apply module about this change using
the methods on the commit & apply context. This context is available
in a global variable called `commitapply` in every mapping file.

Available methods on the commit & apply context are:

- `newset(<path>)` to inform about a new 'set' operation. The `path`
  argument is whatever string the caller chooses. Commit & apply itself
  doesn't care what that string is.
- `newadd(<path>)` to inform about a new 'add' operation.
- `newdelete(<path>)` to inform about a new 'delete' operation.
- `newreorder(<path>)` to inform about a new 'reorder' operation.

The module will then check if any of the rules match this change. If so,
it will queue the corresponding action when the complete request has been
processed and was successful.

When the `apply()` function of the Transformer API is invoked then all
the queued actions will be executed asynchronously in the background.
The same action is never queued twice for execution in the same queue.

There are no guarantees in which order the queued actions are executed.


Rule Files
----------
Rule files must have the file extension `.ca` and must be stored in the
directory identified by the `commitpath` configuration parameter of
Transformer.

Each line in a rule file must consist of a rule and an action, separated
by whitespace:

- A rule is any valid [string pattern][patterns] for use by Lua's
[`string.match()`][match]
  function. It cannot contain any whitespace. When commit & apply is informed
  about a new set, add or delete it will match all rules against the
  input provided by the caller. The callers will have to document what they
  pass as input so the proper rules can be written. See below for what the
  UCI mapper does.
- An action is a string that can be executed as-is and that will cause the
  relevant configuration changes to be applied. This string may contain
  whitespace.

If an invalid rule is found then a trace is printed and that line is ignored.
Subsequent rules and rule files are still processed.

[patterns]: http://www.lua.org/manual/5.1/manual.html#5.4.1 "Lua String Patterns"
[match]: http://www.lua.org/manual/5.1/manual.html#pdf-string.match "string.match()"


UCI Mapper
----------
The UCI mapper has been adapted to inform commit & apply about all changes to
the UCI configuration files. This means that all mappings that use the UCI
mapper don't need to do anything concerning commit & apply. 

The information that the UCI mapper passes to the commit & apply module is
`<config>.<sectiontype>.<sectionname>.<option>` on which the configuration change happens.

- If `<sectiontype>` is unknown, then a ? is used instead of the section type.
- If `<option>` is not relevant (add / delete), then it's not included

An example rule and action to restart `cwmpd` when a change happens in its UCI
config file:
    cwmpd /etc/init.d/cwmpd restart
Strictly speaking the above rule also matches when a change is made on a
*section* called `cwmpd`. When you want the rule to only match when *config* is
`cwmpd` then use a rule pattern like `^cwmpd`.
