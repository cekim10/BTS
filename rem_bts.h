#include "hybridmap.h"
#include "MessageManager.h"
#include <unordered_map>
#include <vector>
#include <iostream>
#include <algorithm>
#include "ska/flat_hash_map.hpp"

#define pure(x) ((x >> 31) ^ x)

using std::swap;

namespace bts{
    void _part(Edge a[], int l, int r, int &i, int &j){
        if (r - l <= 1) {
            if (a[r].v < a[l].v)
                swap(a[r], a[l]);
            i = l;
            j = r;
            return;
        }
        
        int m = l;
        int pivot = a[r].v;
        while (m <= r) {
            if (a[m].v < pivot)
                swap(a[l++], a[m++]);
            else if (a[m].v > pivot)
                swap(a[m], a[r--]);
            else
                m++;
        }
        i = l-1;
        j = m;
    }
    
    void _sort(Edge a[], int l, int r){
        if (l >= r) return;
        
        int i, j;
        _part(a, l, r, i, j);
        _sort(a, l, i);
        _sort(a, j, r);
    }

    class RemInit{
        public:
        int num_nodes, num_parts;
        hash_t p;

        RemInit(int num_nodes, int num_parts): num_nodes(num_nodes), num_parts(num_parts){
        }
        ~RemInit(){
            p.clear();
        }

        int pp(int u){
            auto x = p.find(u);
            return x == p.end() ? u : x->second;
        }

        void merge(int u, int v){
            int z;
            
            int up, vp;
            while((up = pp(u)) != (vp = pp(v))){
                if(up < vp){
                    p[v] = up;
                    v = vp;
                }
                else{
                    p[u] = vp;
                    v = up;
                }
            }
        }

        int find(int u){
            int up = pp(u);
            int upp;
            while(up != (upp = pp(up))){
                p[u] = upp;
                u = up;
                up = upp;
            }
            return up;
        }

        int partition(Edge edges_tmp[]){

            int size = 0;
            for(auto pair : p){
                edges_tmp[size++] = {pair.first, find(pair.second)};
            }

            _sort(edges_tmp, 0, size-1);
            
            int* heads = new int[num_parts];
            
            for(int i = 0, j = 0; i < size;){
                memset(heads, -1, sizeof(int) * num_parts);
                int cc = edges_tmp[i].v;
                heads[cc % num_parts] = cc;
                for(; j < size && edges_tmp[j].v == cc; j++){
                    int jp = edges_tmp[j].u % num_parts;
                    if(heads[jp] > edges_tmp[j].u || heads[jp] == -1) heads[jp] = edges_tmp[j].u;
                }

                for(; i < j; i++){
                    int ip = edges_tmp[i].u % num_parts;
                    if(heads[ip] != edges_tmp[i].u)
                        edges_tmp[i].v = heads[ip];
                }

            }

            delete heads;

            return size; 
        }

        void clear(){
            p.clear();
        }
    };

    class RemBTS{
        public:
        HybridMap p;
        int num_procs;

        RemBTS(int num_nodes, int pid, int num_procs): p(num_nodes, pid, num_procs), num_procs(num_procs){}

        // assume u > v
        void merge(int u, int v){
            int up = pure(p[u]);
            int vp = pure(p[v]);

            if(up == u && vp == v){
                p[u] = vp;
                return;
            }

            while(up != vp){
                if(up < vp){
                    p[v] = ~up;
                    v = vp;
                }
                else{
                    p[u] = ~vp;
                    u = up;
                }
                up = pure(p[u]);
                vp = pure(p[v]);
            }
            
        }

        int find(int u){
            int up = pure(p[u]);
            int upp;
            while(up != (upp = pure(p[up]))){
                p[u] = ~upp;
                u = up;
                up = upp;
            }
            return up;
        }

        /**
         * assume toemit is empty
         */
        int getEdgesToEmit(Edge toemit[], size_t& num_changes){
            
            int size = 0;

            for(const auto &x : p.p_out){
                int pr = find(x.first);
                if(pr != x.first && x.second < 0){
                    toemit[size++] = Edge{x.first, pr};
                    num_changes++;
                }
            }

            _sort(toemit, 0, size-1);
            
            int* heads = new int[num_procs];
            
            for(int i = 0, j = 0; i < size;){
                memset(heads, -1, sizeof(int) * num_procs);
                int cc = toemit[i].v;
                heads[cc % num_procs] = cc;
                for(; j < size && toemit[j].v == cc; ++j){
                    int jp = toemit[j].u % num_procs;
                    if(heads[jp] > toemit[j].u || heads[jp] == -1) heads[jp] = toemit[j].u;
                }

                for(; i < j; ++i){
                    int ip = toemit[i].u % num_procs;
                    if(heads[ip] != toemit[i].u)
                        toemit[i].v = heads[ip];
                }
            }

            delete heads;
            
            int u;
            int up;
            ska::flat_hash_map<int, int> ucc;

            for(int i=0, u=p.pid; u < p.num_nodes; i++, u += p.num_procs){
                up = find(u);

                if(up % p.num_procs == p.pid) continue;

                auto it = ucc.find(up);
                if(it == ucc.end()){
                    ucc[up] = u;
                    toemit[size++] = Edge{u, up};
                    if(p[u] < 0){
                        p[u] = up;
                        num_changes++;
                    }
                }
                else{
                    p[u] = it->second;
                }
            }

            p.p_out.clear();
            
            return size;
        }

        void _sort(Edge a[], int l, int r){
            if (l >= r) return;
            
            int i, j;
            _part(a, l, r, i, j);
            _sort(a, l, i);
            _sort(a, j, r);
        }

        void _part(Edge a[], int l, int r, int &i, int &j){
            if (r - l <= 1) {
                if (a[r].v < a[l].v)
                    swap(a[r], a[l]);
                i = l;
                j = r;
                return;
            }
            
            int m = l;
            int pivot = a[r].v;
            while (m <= r) {
                if (a[m].v < pivot)
                    swap(a[l++], a[m++]);
                else if (a[m].v > pivot)
                    swap(a[m], a[r--]);
                else
                    m++;
            }
            i = l-1;
            j = m;
        }

    };
}