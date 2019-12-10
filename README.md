# DISCLAIMER

Please note this repo is provided `as is` - it's not actively maintained and no support for things that are broken, undocumented and/or unclear is provided.

---

# Efficient DSDV routing module for network simulator ns-3 #

This repository accompanies the thesis *Improving Table-Driven Mobile Ad Hoc Networks through Demand-Driven Features: A comparative simulation of DSDV and Eff-DSDV Routing Protocols* and provides an implementation of the *Efficient Destination-Sequenced Distance Vector (Eff-DSDV)* Routing Protocol as a C++ implementation for the network simulation in ns-3. The modul is based on the DSDV standard module. 

## What is this about? ## 

Finding an optimal path between two devices remains one of the main challenges in Mobile Ad Hoc Networks (MANETs). In such, each device acts both as a router and a network host and thereby connects devices that are not in each other’s service range. The Destination-Sequenced Distance Vector (DSDV) protocol was one of the first routing protocols designed for the special properties of MANETs. With the Efficient Destination-Sequenced Distance Vector (Eff-DSDV) protocol, a more efficient version of the table-driven DSDV has been proposed, claiming to increase the low Packet Delivery Ratio (PDR) of its precursor. However, doubts remain in terms of the repeatability of the results. Thus, this paper investigates the proposed changes by discussing the (dis-) advantages of the protocols routing approaches; implementing and testing of the protocols to conduct a comparative simulation study in the network simulator suite ns-3. It will be shown that Eff-DSDV cannot live up to its name and offers no significant advantage over DSDV. 

## Building the module ##

In order to make use of this implementation, you'll need a recent ns-3 version [installed](https://www.nsnam.org/wiki/Installation), ideally version 3.28. Copy the eff-dsdv folder into your ns-3/src/ directory. Afterwards, you can build the module from the project root using `waf`:

```bash
./waf configure --disable-python --enable-examples --enable-tests
```

## Running tests or examples ##

This project does not use the pytest framework for running tests. Instead, one can run tests using:

```bash
./waf --run "test-runner --suite=eff-dsdv"
```

This command will run the test contained under eff-dsdv/test.

## Running Eff-DSDV simulations ##

The file manet-routing-compare.cc contains a simulation manager allowing to run configuration-based simulation scenarios. 
Copy manet-routing-compare.cc into the project root's scratch/ folder and run the following command from terminal:

```bash
./waf --run="scratch/manet-routing-compare"
```
However, this will only the standard scenario. One can either provide a configuration file 

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
Furthermore, one can retrieve additional output from the Simulation Manager by enabling the extensiveOutput flag. Additionally, it is recommended to print the output to a log file

```bash
./waf --run="scratch/manet-routing-compare --extensiveOutput=1" > log.out 2>&1
```
However, please note that the extensiveOutput flag will print ALOT of information and require extensive amount of disk space. Use with caution. 

## Authors ##

* **Thorben Hellweg** [thllwg](https://github.com/thllwg) 
