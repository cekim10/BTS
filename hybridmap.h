#include "ska/flat_hash_map.hpp"
// #include <memory.h>
// using namespace std;

typedef ska::flat_hash_map<int, int> hash_t;

class HybridMap{
    public:
    int num_local_nodes;
    int pid;
    int num_procs;
    int num_nodes;

    int* p_in;
    hash_t p_out;

    HybridMap(int num_nodes, int pid, int num_procs): pid(pid), num_nodes(num_nodes), num_procs(num_procs){
        num_local_nodes = num_nodes / num_procs + (pid <= num_nodes % num_procs);
        p_in = new int[num_local_nodes];
        for(int i=0; i<num_local_nodes; i++){
            p_in[i] = pid + i * num_procs;
        }
        // memset(p_in, -1, sizeof(int) * num_local_nodes);
    }

    int& operator[] (int u){  
        if(u % num_procs == pid)
            return p_in[u / num_procs];
        else{
            if(p_out.find(u) == p_out.end()){
                p_out[u] = u;
            }
            return p_out[u];
        }
    }

};