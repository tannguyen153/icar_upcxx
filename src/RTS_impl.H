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

typedef upcxx::future<> CommRequest;

struct Message{
  int src, dst;
  volatile int bufSize;
  volatile bool completed; //message transfer is done
  volatile bool served; //message transfer request has been served but may have not completed
#ifdef MULTI_THREADED
  pthread_mutex_t packageLock;
#endif
  int tag;
  CommRequest *request; //!for inter-process communication 
  upcxx::global_ptr<double> databuf;
  Message(){
    databuf = nullptr;
    bufSize = 0;
    src = 0;
    dst = 0;
    completed = false;
    served = false;
    request = 0;
    tag=0;
#ifdef MULTI_THREADED
    packageLock= PTHREAD_MUTEX_INITIALIZER;
#endif
  }
  ~Message(){
    if(databuf!= nullptr)
      if(databuf.is_local())
      {
        upcxx::delete_array(databuf);
      }
  }
  Message(int size){
    databuf = (upcxx::global_ptr<double>)upcxx::new_array<double>(size);
    bufSize = size;
    src = 0;
    dst = 0;
    completed = false;
    served = false;
    request = 0;
    tag=0;
#ifdef MULTI_THREADED
    packageLock= PTHREAD_MUTEX_INITIALIZER;
#endif
  }
  Message(int src, int dest, int size){
    databuf = (upcxx::global_ptr<double>)upcxx::new_array<double>(size);
    bufSize = size;
    src = src;
    dst = dest;
    completed = false;
    served = false;
    request = 0;
    tag=0;
#ifdef MULTI_THREADED
    packageLock= PTHREAD_MUTEX_INITIALIZER;
#endif
  }
  void setSource(int s){src=s;}
  void setDestination(int d){dst=d;}
  void completeRequest(void){
#ifdef MULTI_THREADED
    pthread_mutex_lock(&packageLock);
#endif
    completed = true;
#ifdef MULTI_THREADED
    pthread_mutex_unlock(&packageLock);
#endif
  }

#ifdef MULTI_THREADED
  void completeRequest(bool canAvoidLock){
    if(!canAvoidLock)pthread_mutex_lock(&packageLock);
    completed = true;
    if(!canAvoidLock)pthread_mutex_unlock(&packageLock);
  }
#endif
  bool checkRequest(void){ return completed;}
};

enum TASK_STATE{
    INIT=0,
    RUNNABLE,
    WAITING,
    FINISHED,
    NUM_STATES
};

typedef std::map< int, std::map<int, std::deque<Message*> > >::iterator inputIter;
typedef std::map< int, std::map<int, std::deque<Message*> > >::iterator outputIter;
struct communicationChannel{
    std::map< int, std::map<int, std::deque<Message*> > >channel;
    bool add(int src, int dst, int tag, size_t msgSize){ //add dependency

	if(channel[src][tag]. size()>0) return false; //this dependency already existed
	Message *msg= new Message(src, tag, msgSize);
        msg->src= src;
        msg->dst= dst;
        channel[src][tag].push_back(msg);
	return true;
    }
    Message* get(int partnerID, int tag){ //pull the input
        //assert(channel[partnerID, tag]. size()>0);	
	Message* msg= channel[partnerID][tag].front();
	channel[partnerID][tag].pop_front();
	msg->served=false;//needed when reusing the message
	return msg;
    }
    void put(int partnerID, int tag, Message* msg){ //push the output data out
	channel[partnerID][tag].push_back(msg);	
    }
    bool checkDependencies(){
	for(inputIter it=channel.begin(); it!=channel.end(); it++){
            for(std::map<int, std::deque<Message*> >::iterator it1=(*it).second.begin(); it1!=(*it).second.end(); it1++)
	    {
		if((*it1).second.size()>0)
		    if((*it1).second.front()->completed==false) return false;
	    }
	}
	return true;
   };
};

class IcarTask{
    protected:
    int _id;
    int _localId;
    TASK_STATE _state;
    public:
    communicationChannel input;
    communicationChannel output;
    IcarTask(){_state= INIT;}
    virtual void initialize(){};
    TASK_STATE getState(){return _state;}
    void setID(int id){_id=id;}
    void setLocalID(int id){_localId=id;}
    bool add_input(int partnerID, int tag, size_t size){return input.add(partnerID, _id, tag, size); }
    Message* get(int partnerID, int tag) {return input.get(partnerID, tag); } 
    void put(int partnerID, int tag, Message* msg){ 
	#ifdef PRINT_OUT_EVENTS
	std::cout<<"Task "<<_id<<" sends data to Task "<<partnerID<<std::endl;
	#endif
	msg->src= _id;
	msg->dst= partnerID;
        msg->completed = false;
        msg->served = false;
	output.put(partnerID, tag, msg); 
    }
    bool checkDependencies(){
	return input.checkDependencies();
    };
    virtual void Run(){};
};

template <int DIM, class T>
class IcarGraph{
    private:
	int _nTasks[DIM];
	int _nLocalTasks[DIM];
        int _size;
  	int _lastHeavyLoadProcess;
	int _switchingPoint;
	int _chunksize;
	std::vector<T*> _taskVec;
    public:
	IcarGraph(){
	   for(int d=0; d<DIM; d++){
		_nTasks[d]=0;
		_nLocalTasks[d]=0;
	   }
	   _size=_nTasks[0];
	   for(int d=1; d<DIM; d++) _size*= _nTasks[d];
	}
        std::vector<T*>& getTaskList(){return _taskVec;}
	int Size(){
	   return _size;
	}
        int LocalSize(){
           int s=_nLocalTasks[0];
           for(int d=1; d<DIM; d++) s*= _nLocalTasks[d];
           return s;
        }
        void Create1DIcarGraph(int nTasks){//for 1D task graph
 	   int base, chunkSize;
           _nTasks[0]=nTasks;
	   _chunksize= chunkSize=  nTasks/upcxx::rank_n();
	   if(upcxx::rank_me()< nTasks%upcxx::rank_n()){
		 _nLocalTasks[0]= chunkSize+1;
		 base= upcxx::rank_me()*(_nLocalTasks[0]+1);
	   }else{
		 _nLocalTasks[0]= chunkSize;
		 base= upcxx::rank_me()*chunkSize + nTasks%upcxx::rank_n();
	   }
    	   for(int i=0; i<_nLocalTasks[0]; i++){
		 _taskVec.push_back(new T());
		 _taskVec[i]->setLocalID(i);
		 _taskVec[i]->setID(base+i);
	         _taskVec[i]->initialize();
		 #ifdef PRINT_OUT_EVENTS
		 std::cout<<"Created Task "<<base+i<< " and assigned to process "<< upcxx::rank_me()<<std::endl;
		 #endif
	   }
	   _lastHeavyLoadProcess= _size%upcxx::rank_n()-1;
	   _switchingPoint= (_lastHeavyLoadProcess+1)*(chunkSize+1);
        }
	int taskToProcess(int taskID){
	   if(taskID<_switchingPoint) return taskID/(_chunksize+1);
	   else return _lastHeavyLoadProcess+1+ (taskID-_switchingPoint)/_chunksize; 
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
	std::list<IcarTask*> taskPool;
	std::deque<IcarTask*> waitQueue; //hold tasks that still need input data
	std::deque<IcarTask*> readyQueue; //hold tasks that ready to execute but wait for available processing worker
	std::deque<IcarTask*> execQueue; //hold tasks that assigned to a processing worker
	std::deque<Message*> messageQueue; //hold tasks that assigned to a processing worker

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
	void serviceCommunicationRequest(IcarGraph<1, IcarTask>* graph);
	void Finalize();
//	double Time();
	void Barrier();
};

}
