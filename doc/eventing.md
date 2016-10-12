Eventing in Transformer
=======================

Introduction
------------

Eventing in Transformer is primarily driven by the requirements for notifications in TR-069.
However, notifications on TR-069 do not correspond 1-to-1 to subscriptions and events in
Transformer. This documents describes what is available in Transformer regarding eventing.
This infrastructure is used by cwmpd to provide the notifications required in the TR-069 spec.

Eventing from the client's perspective
--------------------------------------

- Clients can subscribe for events of a certain type. One subscription can cover any
  combination of 'set', 'add' and 'delete' events.
- Subscriptions can be placed on one specific parameter of a specific object or on a whole
  subtree.
- When subscribing the client will also receive the list of parameters that are explicitly
  marked as not evented in the mapping by means of the `activeNotify = "canDeny"`
  attribute.
- Clients can specify whether they want to receive events for changes done by themselves.


Eventing from the mapping's perspective
---------------------------------------

There are three categories of datamodel changes that can lead to events for a subscription
of a specific client:

1. Changes done by other clients through Transformer on the same datamodel.
2. Changes done by other clients through Transformer on another datamodel.
3. Changes due to external factors, for example a device connecting to the gateway and
   receiving a DHCP lease.

For changes of the first type Transformer can easily send out the necessary events because
it has all the necessary information: the new value, the exact object and parameter that
changed and who to notify of the event.

The third type obviously cannot be handled by Transformer without help from the mappings.

The second type might need some further clarifications. The main use case is being able to
send TR-069 notifications (regarding the IGD or Device:2 datamodel) due to changes done by
the end user on the webUI (mainly regarding the UCI datamodel). This type is often referred
to as 'cross-datamodel eventing' or 'XDE'.

For both the second and third type the mappings need to explicitly add support. The
mechanism available for that is the same in both cases and consists of 'event sources'
and 'watchers'.
1. At a certain moment the mapping is triggered by Transformer to deploy its watchers.
2. These watchers can be created and are managed by event sources. There are several
   event sources available; each allowing the mapping to watch for changes in a certain
   part of the system (UCI, ubus).
   To create a watcher the mapping specifies some details on what change to watch out for,
   together with a callback function.
3. Whenever an event source sees a change that matches a watcher it will invoke the
   corresponding callback function. The mapping should then investigate the reported
   change and decide whether to inform Transformer about this change.

More details about this can be found in `./doc/skeleton.map` and the API documentation
of the event sources.

Notes
-----
- Changes done directly in UCI without passing through Transformer can never lead to
  events because Transformer never sees these changes.
- It is not possible to easily list the parameters 'for which eventing is supported'.
  As can be deduced from the above explanations for a particular parameter only events
  of the first type are always supported. For the other types explicit support must be
  provided by the mappings but Transformer has no way of knowing whether a mapping
  does this and for which parameters in particular.
