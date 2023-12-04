# BTS

- BTS is a tool for finding all connected components in large graph.
- Given an undirected graph, BTS connects each vertex to the most preceding vertex in the connected component.
- It runs in a distributed-memory system. 

## Build

Before running BTS, mpicxx must be installed in a cluster.
To build the project, type the following command in terminal:

```bash
mpicxx -o BTS bts.cpp -O3 -lpthread
```

## Run BTS
- Prepare an edge list file of a graph. ex) multiple.tsv
- example (multiple.tsv)
```
0   1
2   3
3   4
4   5
5   6
7   8
8   9
```

- Convert tsv file to binary file
```bash
g++ -o txttobin txttobin.cpp
./txttobin input.tsv input.bin
```

- Prepare hosts file: list all the machines to the file
- example (hosts)
```
n00
n01
n02
n03
```

- Execute BTS
```bash
mpirun -np numberofprocessors --hostfile yourhostsfile ./BTS /input/path/ maximumvertexvalue
```
- example
```bash
mpirun -np 10 --hostfile hosts ./BTS test_dataset/grqc.bin 5242
```

## Datasets
### Real world graphs
| Name        | #Nodes      | #Edges        | Description                                                 | Source                           |
|-------------|-------------|---------------|-------------------------------------------------------------|----------------------------------|
| LiveJournal     | 4,847,571  | 68,993,773 | LiveJournal online social network                           | [SNAP](http://snap.stanford.edu/data/soc-LiveJournal1.html) |
| Twitter     | 41,652,230  | 1,468,365,182 | Twitter follower-followee network                           | [Advanced Networking Lab at KAIST](http://an.kaist.ac.kr/traces/WWW2010.html) |
| Friendster  | 65,608,366  | 1,806,067,135 | Friendster online social network                            | [SNAP](http://snap.stanford.edu/data/com-Friendster.html)                             |
| SubDomain   | 89,247,739  | 2,043,203,933 | Domain level hyperlink network on the Web                   | [Yahoo Webscope](http://webdatacommons.org/hyperlinkgraph/)                   |
| gsh-2015    | 988,490,691 | 33,877,399,152 | Page level hyperlink network on the Web                     | [WebGraph](http://law.di.unimi.it/webdata/gsh-2015/) 

### Synthetic graphs
RMAT-k for k ∈ {21, 23, 25, 27, 29, 31} is a synthetic graph following RMAT model.
We generate RMAT-k graphs using [TegViz](https://datalab.snu.ac.kr/tegviz/).
We set RMAT parameters (a, b, c, d) to (0.57, 0.19, 0.19, 0.05).
| Name      | #Nodes      | #Edges        |
|-----------|-------------|---------------|
| RMAT-21 | 1,114,816 | 31,457,280 |
| RMAT-23 | 4,120,785 | 125,829,120 |
| RMAT-25 | 15,212,447 | 503,316,480 |
| RMAT-27 | 56,102,002 | 2,013,265,920 |
| RMAT-29 | 207,010,037| 8,053,063,680 |
| RMAT-31 | 762,829,446 | 32,212,254,720 |
