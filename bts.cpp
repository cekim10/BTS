#include <mpi.h>
#include <stdlib.h>
#include <thread>
#include <iostream>
#include <chrono>
#include "rem_bts.h"

#include "MessageManager.h"
#include "ska/flat_hash_map.hpp"
using namespace std;
using namespace std::chrono;
using namespace bts;

#define read_buffer_size 1024
#define recv_buffer_size 1024

Edge* read_buffer = new Edge[read_buffer_size];
Edge* recv_buffer = new Edge[recv_buffer_size];

typedef ska::flat_hash_map<int, int> hash_t;

MPI_Datatype dt_edge;


void receiver(int num_procs, MessageManager* msgmng, RemBTS* rembts, size_t* total_received_size){
    int remain = num_procs;

    MPI_Status status;
    int received_size;
    *total_received_size = 0;

    while(remain){
        MPI_Recv(recv_buffer, msgmng->capacity, dt_edge, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, dt_edge, &received_size);

        for(int i=0; i<received_size; i++){
            rembts->merge(recv_buffer[i].u, recv_buffer[i].v);
        }

        *total_received_size += received_size;
        
        if(status.MPI_TAG == EndMessage) remain--;
    }
}

void simply_emitting(int pid, int num_edges, int num_procs, MessageManager* msgmng, FILE* f){
    size_t num_read_edges = 0;
    for(int i=0; i<num_edges;){

        num_read_edges = num_edges - i < read_buffer_size ? num_edges - i : read_buffer_size;
        num_read_edges = fread(read_buffer, sizeof(Edge), num_read_edges, f);
        
        for(int j=0; j<num_read_edges; j++){
            Edge e = read_buffer[j];
            if(e.u < e.v){
                e = Edge{e.v, e.u};
            }
            int up = e.u % num_procs;
            int vp = e.v % num_procs;
            msgmng->emit(&e, up);
            if(up != vp){
                msgmng->emit(&e, vp);
            }
        }
        i += num_read_edges;
    }

    for (int step = 0; step < num_procs; step++) {
        int id_dst = (pid - step + num_procs) % num_procs;
        msgmng->flush(id_dst);
        msgmng->endAck(id_dst);
    }
}

void initialization_step(int pid, int num_nodes, int num_edges, int num_procs, Edge* emit_buffer, int emit_buffer_size, MessageManager* msgmng, FILE* f){

    RemInit* reminit = new RemInit(num_nodes, num_procs);

    size_t num_read_edges = 0;
    for(int i=0; i<num_edges;){
        while(reminit->p.size() + read_buffer_size <= emit_buffer_size && i < num_edges){
            num_read_edges = num_edges - i < read_buffer_size ? num_edges - i : read_buffer_size;
            num_read_edges = fread(read_buffer, sizeof(Edge), num_read_edges, f);
            for(int j=0; j<num_read_edges; j++){
                reminit->merge(read_buffer[j].u, read_buffer[j].v);
            }
            i += num_read_edges;
        }

        int size_to_emit = reminit->partition(emit_buffer);
        
        for(int j=0; j<size_to_emit; j++){
            int up = emit_buffer[j].u % num_procs;
            int vp = emit_buffer[j].v % num_procs;

            msgmng->emit(&emit_buffer[j], up);
            if(up != vp){
                msgmng->emit(&emit_buffer[j], vp);
            }
        }

        reminit->clear();
    }

    for (int step = 0; step < num_procs; step++) {
        int id_dst = (pid - step + num_procs) % num_procs;
        msgmng->flush(id_dst);
        msgmng->endAck(id_dst);
    }
    
    delete reminit;
}

FILE* open_and_seek(char* input, int pid, int num_procs, long& num_edges){
    FILE* f = fopen(input, "rb");
    fseek(f, 0, SEEK_END);
    long num_total_edges = ftell(f) / sizeof(Edge);
    long pos_start = (num_total_edges * pid / num_procs) * sizeof(Edge);
    num_edges = (num_total_edges * (pid+1) / num_procs) - (num_total_edges * (pid) / num_procs);
    fseek(f, pos_start, SEEK_SET);

    fprintf(stderr, "(%d/%d) num_total_edges: %ld, pos_start: %ld, num_edges: %ld\n", pid, num_procs, num_total_edges, pos_start, num_edges);
    return f;
}

int main(int argc, char** argv){

    auto start = high_resolution_clock::now();

    int provided, num_procs, pid;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &pid);
    MPI_Type_contiguous(2, MPI_INT, &dt_edge);
    MPI_Type_commit(&dt_edge);

    char* input = argv[1];
    int num_nodes = strtol(argv[2], NULL, 10);
    fprintf(stderr, "(%d/%d) num_nodes: %d\n", pid, num_procs, num_nodes);

    int emit_buffer_size = (num_nodes/num_procs + 1)*2;
    if(emit_buffer_size < read_buffer_size)
        emit_buffer_size = read_buffer_size;
    
    Edge* emit_buffer = new Edge[emit_buffer_size];
    MessageManager* msgmng = new MessageManager(num_procs, dt_edge);
    RemBTS* rembts = new RemBTS(num_nodes, pid, num_procs);
    size_t total_communication, total_received_size;

    long num_edges;
    FILE* f = open_and_seek(input, pid, num_procs, num_edges);
    thread recv_thread(receiver, num_procs, msgmng, rembts, &total_received_size);
    // simply_emitting(pid, num_edges, num_procs, msgmng, f);
    initialization_step(pid, num_nodes, num_edges, num_procs, emit_buffer, emit_buffer_size, msgmng, f);
    fclose(f);

    milliseconds elapsed_ms = duration_cast<milliseconds>(high_resolution_clock::now()-start);
    recv_thread.join();
    fprintf(stderr, "(Intialize, pid %d) recv %ld, %ld ms.\n", pid, total_received_size, elapsed_ms.count());

    MPI_Allreduce(&total_received_size, &total_communication, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
    
    int round = 1;
    size_t num_changes = 1;
    size_t num_total_changes = 1;
    while(num_total_changes){
        auto timer1 = high_resolution_clock::now();
        num_changes = 0;
        int toemit_size = rembts->getEdgesToEmit(emit_buffer, num_changes);
        int64_t t1 = duration_cast<milliseconds>(high_resolution_clock::now()-timer1).count();
        
        MPI_Barrier(MPI_COMM_WORLD);

        auto timer2 = high_resolution_clock::now();

        thread recv_thread(receiver, num_procs, msgmng, rembts, &total_received_size);

        for(int j=0; j<toemit_size; j++){
            int up = emit_buffer[j].u % num_procs;
            int vp = emit_buffer[j].v % num_procs;
            if(up != pid){
                msgmng->emit(&emit_buffer[j], up);
            }
            if(vp != pid && up != vp){
                msgmng->emit(&emit_buffer[j], vp);
            }
        }

        for (int step = 0; step < num_procs; step++) {
            int id_dst = (pid - step + num_procs) % num_procs;
            msgmng->flush(id_dst);
            msgmng->endAck(id_dst);
        }
        
        int64_t t2 = duration_cast<milliseconds>(high_resolution_clock::now()-timer2).count();

        recv_thread.join();
        
        MPI_Allreduce(&total_received_size, &total_communication, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
        MPI_Allreduce(&num_changes, &num_total_changes, 1, MPI_UNSIGNED_LONG, MPI_SUM, MPI_COMM_WORLD);
        
        int64_t t3 = duration_cast<milliseconds>(high_resolution_clock::now()-timer1).count();

        fprintf(stderr, "(Round %d, pid %d) recv %ld, changes %ld, refine: %ld ms, send: %ld ms, total: %ld ms.\n", round++, pid, total_received_size, num_total_changes, t1, t2, t3);
        
    }

    for(int i = pid; i < num_nodes; i += num_procs){
        rembts->find(i);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();
}
