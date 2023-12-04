#ifndef MESSAGEMANAGER_H_
#define MESSAGEMANAGER_H_

#include <memory.h>
#include <mpi.h>

struct Edge{
	int u, v;
	void set(int u, int v){
		this->u = u;
		this->v = v;
	}
};

enum MessageTag{
	PassMessage,
	EndMessage
};

class MessageManager {
private:
	int* cnts; // bytes = cnts * sizeof(Edge)
	Edge** buffer;
	int num_procs;
	MPI_Datatype dt_edge;


public:
	const size_t capacity = 1024; // bytes = capacity * sizeof(Edge)
	size_t size_sent = 0;

	MessageManager(int num_procs, MPI_Datatype dt_edge): num_procs(num_procs), dt_edge(dt_edge){
		buffer = new Edge*[num_procs];
		for(int i=0; i<num_procs; i++){
			buffer[i] = new Edge[capacity];
		}
		cnts = new int[num_procs]();
	}

	~MessageManager(){
		for(int i=0; i<num_procs; i++)
			delete[] buffer[i];
		delete[] buffer;
		delete[] cnts;
	}

	void emit(Edge* e, int id_dst){
		buffer[id_dst][cnts[id_dst]++] = *e;

		if(cnts[id_dst] == capacity){
			flush(id_dst);
		}
	}

	void flush(int id_dst){
		if(cnts[id_dst]){
			MPI_Send(buffer[id_dst], cnts[id_dst], dt_edge, id_dst, PassMessage, MPI_COMM_WORLD);
			size_sent += cnts[id_dst];
			cnts[id_dst] = 0;
		}
	}

	void endAck(int id_dst){
		MPI_Send(buffer[id_dst], 0, dt_edge, id_dst, EndMessage, MPI_COMM_WORLD);
	}

	void clear(){
		size_sent = 0;
		memset(cnts, 0, sizeof(int) * num_procs);
	}
};

#endif /* MESSAGEMANAGER_H_ */
