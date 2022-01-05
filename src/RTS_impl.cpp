#include "RTS_impl.H"
#include <upcxx/upcxx.hpp>
#include <deque>
namespace ICAR_UPCXX{
    struct RtsDomain{
        pthread_t *_threads;
        int _size;
        MyLock _lock;
        RtsDomain():_threads(NULL), _size(0){};
        ~RtsDomain(){
            free(_threads);
        }
    };
    int numa_nodes;
    RtsDomain *dom;
    MyLock _l;
    volatile char startSignal=0;
    pthread_mutex_t startLock= PTHREAD_MUTEX_INITIALIZER;

    int RTS::ProcCount(){
        return _nProcs;
    }

    int RTS::MyProc(){
        return _rank;
    }

    int RTS::WorkerThreadCount(){
        return _nWrks;
    }

    int RTS::MyWorkerThread(){
        return 0;
    }

    struct argT {
        int numaID;
        int tid;
        int g_tid;
        int nThreads;
        int nTotalThreads;
        int max_step;
        double stop_time;
        RTS* thisRTS;
    };

#ifdef USE_MPI_LAUNCHER
    void Run(void* threadInfo){
        argT *args= (argT*)threadInfo;
        int numaID= args->numaID;
        int tid= args->tid;
        int g_tid= args->g_tid;
        int nThreads= args->nThreads;
        int nTotalThreads= args->nTotalThreads;
        int max_step= args->max_step;
        double stop_time= args->stop_time;
        RTS* rts= args->thisRTS;
        ICAR_UPCXX::registerId(g_tid);
        //done with thread id setup, now wait for the start signal from master
        pthread_mutex_lock(&startLock);
        startSignal++;
        pthread_mutex_unlock(&startLock);
        while(startSignal!= nTotalThreads){}
        rts->runTask();
    }

    void InitializeMPI(){
        int provided;
        MPI_Init_thread(0, 0, MPI_THREAD_FUNNELED, &provided);
        if(provided == MPI_THREAD_SINGLE){//with this MPI, process can't spawn threads
            cerr << "Spawning threads is not allowed by the MPI implementation" << std::endl;;
        }
    }
#endif

    void RTS::RTS_Init(){
    }

    void RTS::Init(){
	_rank=0;
	_nProcs=1;
#ifdef USE_MPI_LAUNCHER
        InitializeMPI();
        MPI_Comm_rank(MPI_COMM_WORLD, &_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &_nProcs);
#endif
        RTS_Init();
    }

    void RTS::Init(int rank, int nProcs){
        _rank= rank;
        _nProcs= nProcs;
        RTS_Init();
    }

    void RTS::Finalize(){
#ifdef ENABLE_DEBUG
        memcheck.report();
#endif
    }

    void RTS::Run(IcarGraph<1, IcarTask>* graph){
        int nTasks= graph->LocalSize();  
	std::deque<IcarTask*> execQueue;
	std::deque<IcarTask*> waitQueue;
	std::deque<IcarTask*> readyQueue;
	for(int i=0; i<nTasks; i++) {
	    IcarTask* t= graph->getTaskList()[i]; 
            switch(t->getState()){
		    case RUNNABLE: readyQueue.push_back(t); break; 
		    case WAITING:  waitQueue.push_back(t); break; 
	    };
	}
	int finishedTasks=0;
        while(true){
	    //serve runnable tasks
	    while(readyQueue.size()>0){
		IcarTask* t=readyQueue.front();
		readyQueue.pop_front();
		t->Run();
		if(t->getState()==RUNNABLE)readyQueue.push_back(t);
                else if(t->getState()==WAITING)waitQueue.push_back(t);
		     else if(t->getState()==FINISHED) finishedTasks++;
	    }
	    //check if any tasks just become runnable
	     //To be implemented

 	    //check if all tasks have finished
	    if(finishedTasks==nTasks) break;
	}
    }

    void RTS::Barrier(){
        upcxx::barrier();
    }
}

