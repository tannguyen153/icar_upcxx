#include "config.H"
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "mylock.h"
#include <stdlib.h>
#include <upcxx/upcxx.hpp>
#include <vector>

namespace ICAR_UPCXX{

struct _workerThreadInfo{
    int _tid; //thread id in local group
    int _size; //number of threads in the group
};

struct _threadInfo{
    bool _isComm; //whether this thread handles communication
    int _wtid; //worker thread id (-1 if this thread is decicated to communication)
    int _nWts; //number of thread groups
};

class Message{
    int dest;
    int tag;
    char* content;
};

class dependency{
    std::vector<Message*> inMessages;  
    public:
        bool eval(){return inMessages.size()>0;} 
        std::vector<Message*>& getDependency(){return inMessages;} 
};

enum TASK_STATE{
    INIT=0,
    RUNNABLE,
    WAITING,
    FINISHED,
    NUM_STATES
};

class IcarTask{
    protected:
    int _id;
    int _localId;
    TASK_STATE _state;
    std::vector<dependency*> depList;
    public:
    IcarTask(){_state= INIT;}
    TASK_STATE getState(){return _state;}
    void setID(int id){_id=id; }
    void setLocalID(int id){_localId=id;}
    bool checkDependencies(){
	bool fireable=true;
	for(int i=0; i<depList.size(); i++){
	    if(depList[i]->eval()) fireable=false;
	}
	return fireable;
    };
    virtual void Run(){};
};

template <int DIM, class T>
class IcarGraph{
    private:
	int _nTasks[DIM];
	int _nLocalTasks[DIM];
	std::vector<T*> _taskVec;
    public:
	IcarGraph(){
	   for(int d=0; d<DIM; d++){
		_nTasks[d]=0;
		_nLocalTasks[d]=0;
	   }
	}
        std::vector<T*>& getTaskList(){return _taskVec;}
	int Size(){
	   int s=_nTasks[0];
	   for(int d=1; d<DIM; d++) s*= _nTasks[d];
	   return s;
	}
        int LocalSize(){
           int s=_nLocalTasks[0];
           for(int d=1; d<DIM; d++) s*= _nLocalTasks[d];
           return s;
        }
        void Create1DIcarGraph(int nTasks){//for 1D task graph
 	   int base, chunkSize;
           _nTasks[0]=nTasks;
	   chunkSize=  nTasks/upcxx::rank_n();
	   if(upcxx::rank_me()< nTasks%upcxx::rank_n()){
		 _nLocalTasks[0]= chunkSize+1;
		 base= upcxx::rank_me()*(_nLocalTasks[0]+1);
	   }else{
		 _nLocalTasks[0]= chunkSize;
		 base= upcxx::rank_me()*chunkSize + nTasks%upcxx::rank_n();
	   }
    	   for(int i=0; i<_nLocalTasks[0]; i++){
		 _taskVec.push_back(new T);
		 _taskVec[i]->setLocalID(i);
		 _taskVec[i]->setID(base+i);
	   }
        }
        ~IcarGraph(){
    	   for(int i=0; i<_nLocalTasks[0]; i++) free(_taskVec[i]);
	   _taskVec.clear();
	}
};

class RTS
{
    private:
	int _nWrks;
	void RTS_Init();
	int _rank, _nProcs;

    public:
	RTS(){
	    _nWrks=1;
	    char* nWrks= getenv("NWORKERS");
	    if(nWrks) _nWrks= atoi(nWrks);
	}
	RTS(int nWrks):_nWrks(nWrks){}
	int ProcCount();
	int MyProc();
	int WorkerThreadCount();
	int MyWorkerThread();
	void Init(); //Build the runtime system from scratch
	void Init(int rank, int nProcs);//Build the runtime system on pre-existing MPI processes
	void Iterate(void *graph, int max_step, double stop_time);
        void Run(IcarGraph<1, IcarTask>* graph);
	void Finalize();
//	double Time();
	void Barrier();
};

}
