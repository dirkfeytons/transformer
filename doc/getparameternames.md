GetParameterNames
=================

GPN in TR-069
-------------
The TR-069 specification describes a GetParameterNames RPC. Depending on the
type of path and the value of the boolean `nextlevel` parameter different
results are expected. Here's an overview (SI = single instance, MI = multi instance):

                     +----------------------------------+-----------------------------------+
                     |        nextlevel = true          |         nextlevel = false         |
    +----------------+----------------------------------+-----------------------------------+
    |                | invalid, must return error       |                                   |
    |   exact path   | code 9003                        | parameter from path               |
    |                |                                  |                                   |
    +----------------+----------------------------------+-----------------------------------+
    |                | all parameters of the instance + | instance + all parameters of the  |
    |  partial path  | direct child instances (for SI   | instance + recursively all child  |
    |  (instance)    | object types) or child object    | object types, instances and       |
    |                | types (for MI object types)      | their parameters                  |
    +----------------+----------------------------------+-----------------------------------+
    |                |                                  | object type + all instances and   |
    |  partial path  | all instances                    | their parameters + recursively    |
    |  (object type) |                                  | all child object types, instances |
    |                |                                  | and their parameters              |
    +----------------+----------------------------------+-----------------------------------+
    |   empty path   | root node                        | everything                        |
    +----------------+----------------------------------+-----------------------------------+

GPN in Transformer
------------------
The Transformer API for GetParameterNames is slightly different: it doesn't take
a boolean `nextlevel` parameter but a number `level` parameter that can have the
value 0, 1 or 2.

The values 1 and 2 correspond directly to `nextlevel` being true and false.
The value 0 is used by cwmpd to properly handle the empty path case. While
cwmpd knows the root node of its datamodel, Transformer can have multiple root
nodes. If Transformer would receive an empty path it wouldn't know which of the
root nodes to return.

So within Transformer you get the following results based on the value of the
`level` parameter:

                     +----------------+----------------------------------+-----------------------------------+
                     |   level = 0    |            level = 1             |             level = 2             |
    +----------------+----------------+----------------------------------+-----------------------------------+
    |                | invalid, error |                                  |                                   |
    |   exact path   | code 9003      | invalid, error code 9003         | parameter from path               |
    |                |                |                                  |                                   |
    +----------------+----------------+----------------------------------+-----------------------------------+
    |                |                | all parameters of the instance + | instance + all parameters of the  |
    |  partial path  | instance       | direct child instances (for SI   | instance + recursively all child  |
    |  (instance)    |                | object types) or child object    | object types, instances and       |
    |                |                | type (for MI object types)       | their parameters                  |
    +----------------+----------------+----------------------------------+-----------------------------------+
    |                |                |                                  | object type + all instances and   |
    |  partial path  | invalid, error | all instances                    | their parameters + recursively    |
    |  (object type) | code 9003      |                                  | all child object types, instances |
    |                |                |                                  | and their parameters              |
    +----------------+----------------+----------------------------------+-----------------------------------+

Use of `level` in Transformer
-----------------------------
The navigation code of Transformer is common for all types of requests. It is
written in a recursive way to start from the given path and descend in the tree
(depth-first). It uses the `level` value to decide when to stop the iteration.
For GetParameterNames requests it uses the value provided in the request; for
other requests Transformer assigns a value itself to make the navigation do the
correct thing for that request.

For example, an `add` request is done on a partial path that points to an MI
object type. The navigation code should only return that object type and then
stop the iteration. To accomplish that Transformer will pass a `level` of 0 when
starting the iteration.