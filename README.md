# Efficient DSDV routing module for network simulator ns-3 #

This repository accompanies the bachelor thesis *Improving Table-Driven Mobile Ad Hoc Networks through Demand-Driven Features: A comparative simulation of DSDV and Eff-DSDV Routing Protocols*. It contains a C++ implementation of the *[Efficient Destination-Sequenced Distance Vector (Eff-DSDV)](https://www.researchgate.net/profile/Rafi-U-Zaman/publication/4375189_An_Efficient_DSDV_Routing_Protocol_for_Wireless_Mobile_Ad_Hoc_Networks_and_its_Performance_Comparison/links/0fcfd50572a7c67ed2000000/An-Efficient-DSDV-Routing-Protocol-for-Wireless-Mobile-Ad-Hoc-Networks-and-its-Performance-Comparison.pdf)* Routing Protocol, written for the [ns-3 network simulator](https://www.nsnam.org/). 

## What is this about? ## 

Finding an optimal (e.g. shortest) path between two devices remains one of the main challenges in Mobile Ad Hoc Networks (MANETs). In such, each device acts both as a router and a network host and thereby connects devices that are not in each other’s service range. The [Destination-Sequenced Distance Vector (DSDV)](https://www.cse.iitb.ac.in/~mythili/teaching/cs653_spring2014/references/dsdv.pdf) protocol was one of the first routing protocols designed for the special properties of MANETs. With the [Efficient Destination-Sequenced Distance Vector (Eff-DSDV)](https://www.researchgate.net/profile/Rafi-U-Zaman/publication/4375189_An_Efficient_DSDV_Routing_Protocol_for_Wireless_Mobile_Ad_Hoc_Networks_and_its_Performance_Comparison/links/0fcfd50572a7c67ed2000000/An-Efficient-DSDV-Routing-Protocol-for-Wireless-Mobile-Ad-Hoc-Networks-and-its-Performance-Comparison.pdf) protocol, an allegedly more efficient version of the table-driven DSDV has been proposed. In particular, Eff-DSDV claims to increase the low Packet Delivery Ratio (PDR) of DSDV. My thesis investigates the proposed changes by discussing the (dis-) advantages of the different routing approaches. I further implemented Eff-DSDV to conduct a comparative simulation study in the network simulator suite ns-3. My thesis shows that Eff-DSDV cannot live up to its name and offers no significant advantage over DSDV. 

## Building the module ##

In order to make use of this implementation, you'll need to [install ns-3](https://www.nsnam.org/wiki/Installation) in version 3.28 (more recent version might work but are untested). Then, copy the content of `src/` into your local `ns-3/src/` directory. Afterwards, you can build the module from the project root using `waf`:

```bash
./waf configure --disable-python --enable-examples --enable-tests
```

## Running tests or examples ##

This project does not use the pytest framework for running tests. Instead, one can run tests using:

```bash
./waf --run "test-runner --suite=eff-dsdv"
```

This command will run the test contained in `eff-dsdv/test`.

## Running Eff-DSDV simulations ##

The file `manet-routing-compare.cc` contains a simulation manager allowing to run configuration-based simulation scenarios. 
Copy `manet-routing-compare.cc` into the project root's `scratch/` folder and run the following command from terminal:

```bash
./waf --run="scratch/manet-routing-compare"
```
However, this will only execute the standard scenario. Alternatively, one can provide a configuration file 

```bash
./waf --run="scratch/manet-routing-compare --configFile=someConfig.csv"
```
or pass arguments directly:

```bash
./waf --run="scratch/manet-routing-compare --nWifis=10 --nSinks=10 --protocol=5 --totalTime=400"
```
To retrieve a list of all possible commands, execute

```bash
./waf --run="scratch/manet-routing-compare --PrintAttributes"
```
Simulation results will be stored in the projects root as a .csv file.
## Debugging / Logging ##

The programm makes heavy use of ns-3 logging capabilities. First, define for which component logging should be enabled, e.g. Eff-DSDV Protocol:

```bash
export NS_LOG=EffDsdvRoutingProtocol=level_debug
```
Furthermore, one can retrieve additional output from the Simulation Manager by enabling the `extensiveOutput` flag. It is recommended to print the output to a log file

```bash
./waf --run="scratch/manet-routing-compare --extensiveOutput=1" > log.out 2>&1
```

## Author ##

**[Thorben Hellweg](https://github.com/thllwg)**
