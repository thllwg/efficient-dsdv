.. include:: replace.txt

Eff-DSDV Routing
------------

Efficient Destination-Sequenced Distance Vector (Eff-DSDV) routing protocol is a pro-active, table-driven routing protocol with reactive features
for MANETs developed by Khan et al. in 2008 and based on Perkins et al. DSDV routing protocol from 1994. It uses hop count as a metric for route
selection.

This model was developed by 
`Thorben Ole Hellweg <http://thllwg.de>`_
at the University of Münster as part of his bachelor thesis.

Eff-DSDV Routing Overview
*********************

Eff-DSDV Main Routing Table: Every node will maintain a table listing all the other nodes it has known either directly
or through some neighbors. Every node has a single entry in the routing table. The entry will store information
about the node's IP address, last known sequence number and the hop count to reach that node. Along with these
details the table also keeps track of the nexthop neighbor to reach the destination node, the timestamp of the last
update received for that node. Using the timestamp of the last received update, the protocol distinguished between valid 
and invalid (stale) routes. 

Standard DSDV update message consists of three fields, Destination Address, Sequence Number and Hop Count and a type header for message type identification purposes.

Each node uses 2 mechanisms to send out the DSDV updates. They are,

1. Periodic Updates
    Periodic updates are sent out after every m_periodicUpdateInterval(default:15s). In this update the node broadcasts
    out its entire routing table.
2. Trigger Updates
    Trigger Updates are small updates in-between the periodic updates. These updates are sent out whenever a node
    receives a DSDV packet that caused a change in its routing table. The original paper did not clearly mention
    when for what change in the table should a DSDV update be sent out. The current implemntation sends out an update
    irrespective of the change in the routing table.

The updates are accepted based on the metric for a particular node. The first factor determinig the acceptance of
an update is the sequence number. It has to accept the update if the sequence number of the update message is higher
irrespective of the metric. If the update with same sequence number is received, then the update with least metric
(hopCount) is given precedence.

In highly mobile scenarios, there is a high chance of route fluctuations, thus we have the concept of weighted
settling time where an update with change in metric will not be advertised to neighbors. The node waits for
the settling time to make sure that it did not receive the update from its old neighbor before sending out
that update.

The current implementation covers all the above features of DSDV. The current implementation also has a request queue
to buffer packets that have no routes to destination. The default is set to buffer up to 5 packets per destination.

Detecting a stale route, the node invalidates the entry and broadcasts a Route Request (RREQ) towards its neighbours, 
specifying to which node a route is requested. While it waits for responses, any traffic directed to the invalidated 
destination is buffered (subject to storage availability). Any neighbour, which has an applicable route to the required 
destination in its routing table answers the RREQ with a Route Acknowledgement (RACK), containing the hop count and the 
time, at which the route was updated lastly.  

The requesting node chooses the best route of all Route Acknowledgements by first comparing the hop count metric and 
then - in case of equal hop counts - the update time of each route. Of those acknowledgements with the best (equal) hop 
counts, the most recently updated one gets chosen.

