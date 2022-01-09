#include "RTS_impl.H"
#include <upcxx/upcxx.hpp>
#include <deque>

namespace ICAR_UPCXX{
#if 1
struct sMsgMap_t{
    std::map< int, std::map< int,  std::list< Message* > > > map;
    volatile int size=0;
#ifdef MULTI_THREADED
    pthread_mutex_t lock= PTHREAD_MUTEX_INITIALIZER;
#endif
}sMsgMap;

struct rMsgMap_t{
    std::map< int, std::map< int,  std::list< Message* > > > map;
    volatile int size=0;
#ifdef MULTI_THREADED
    pthread_mutex_t lock= PTHREAD_MUTEX_INITIALIZER;
#endif
}rMsgMap;

struct getReq_t{
    int src;
    int tag;
    upcxx::global_ptr<double> sbuf;
    int size;
    getReq_t(int _src, int _tag, upcxx::global_ptr<double> _sbuf, int _size):src(_src), tag(_tag), sbuf(_sbuf), size(_size){}
};

struct pendingGetList_t{
    std::list< getReq_t* > _pendingGets;
#ifdef MULTI_THREADED
    pthread_mutex_t lock= PTHREAD_MUTEX_INITIALIZER;
#endif
    void add(getReq_t* req){
#ifdef MULTI_THREADED
         pthread_mutex_lock(&lock);
#endif
        _pendingGets.push_back(req);
#ifdef MULTI_THREADED
         pthread_mutex_unlock(&lock);
#endif
    }
    void process(){
        if(_pendingGets.size()==0) return;
#ifdef MULTI_THREADED
        pthread_mutex_lock(&(rMsgMap.lock));
        pthread_mutex_lock(&lock);
#endif
        std::list< getReq_t* >::iterator it= _pendingGets.begin();
        while(it != _pendingGets.end()){
            double* localbuf= NULL;
            int src= (*it)->src;
            int tag= (*it)->tag;
            if(rMsgMap.map.find(src) != rMsgMap.map.end()){
                if(rMsgMap.map[src].find(tag) != rMsgMap.map[src].end()){
                   if(rMsgMap.map[src][tag].size() >0){
                       rMsgMap.map[src][tag].front()->tag= tag;
                       localbuf= (rMsgMap.map[src][tag].front()->databuf).local(); //(double*) (static_cast<upcxx::global_ptr<void> > (rMsgMap.map[src][tag].front()->databuf).local());
                       *(rMsgMap.map[src][tag].front()->request)= upcxx::rget((*it)->sbuf, localbuf, (*it)->size);
                       //rMsgMap.map[src][tag].pop_front();
                       //rMsgMap.size--;
                       std::list< getReq_t* >::iterator it1= it;
                       it++;
                       delete (*it);
                       _pendingGets.erase(it1);
                   }else it++;
                }else it++;
            }else it++;
        }
#ifdef MULTI_THREADED
        pthread_mutex_unlock(&lock);
        pthread_mutex_unlock(&(rMsgMap.lock));
#endif
    }
} pendingGetList;

void RTS::serviceCommunicationRequest(IcarGraph<1, IcarTask>* graph)
{
    int myProc= _rank;
    int np= _nProcs; 
    bool nextsReq, nextrReq;
    int numCommTasks = waitQueue.size();
    for(int t=0; t<numCommTasks; t++){
	IcarTask* task= waitQueue.front();  
	waitQueue.pop_front();
        for(inputIter it=task->input.channel.begin(); it!=task->input.channel.end(); it++){
	    int src= (*it).first;
            for(std::map<int, std::deque<Message*> >::iterator it1=(*it).second.begin(); it1!=(*it).second.end(); it1++)
	    {
		int tag= (*it1).first;
                if((*it1).second.size()==1)
                    nextrReq = true;
                else
                {
                    Message *msg = (*it1).second.back();
                    if(msg->completed && (*it1).second.size() == 2) //!latest receive request has been completed
                        nextrReq = true;
                    else //!expected message is still on the way
                        nextrReq = false;
                }
                if(nextrReq) //post a receive
                {
                    Message *msg     = (*it1).second.back();
                    msg->request = new upcxx::future<>;
                    msg->tag = tag;
                    rMsgMap.map[src][tag].push_back(msg);
                    rMsgMap.size++;
                }
	    }
        }
	waitQueue.push_back(task);
    }

    std::vector<IcarTask*> taskList= graph->getTaskList();
    for(int t=0; t<taskList.size(); t++)
    {   
	IcarTask* task= taskList[t];
        for(outputIter it=task->output.channel.begin(); it!=task->output.channel.end(); it++){
	    int dst= (*it).first;
            for(std::map<int, std::deque<Message*> >::iterator it1=(*it).second.begin(); it1!=(*it).second.end(); it1++)
	    {
		int tag= (*it1).first;
                if((*it1).second.size() == 0) //then !no message has been issued or all send requests have been fulfilled
                    nextsReq = false;
                else
                    nextsReq = true;
                if(nextsReq)
                {   
                    Message *sMessage = (*it1).second.front();
                    if(!sMessage->served)
                    {   
                        sMessage->completed = false;
                        sMessage->served = true;
                        int src= sMessage->src;
                        //register send request so that the receiver can send back confirmation upon pull completion
                        sMessage->completed = false;
                        sMsgMap.map[dst][tag].push_back(sMessage);
                        sMsgMap.size++;
                        int size= sMessage->bufSize;
                        upcxx::global_ptr<double> sbuf= sMessage->databuf; //static_cast<upcxx::global_ptr<double> >((double*)sMessage->databuf);
                        upcxx::rpc(graph->taskToProcess(dst),
                         [=](){
                            //at destination rank, look up recv buffer and pull remote data and store data in the buffer
                            bool posted_recv=false;
                            double* localbuf= NULL;
                            if(rMsgMap.map.find(src) != rMsgMap.map.end()){
                              if(rMsgMap.map[src].find(tag) != rMsgMap.map[src].end())
                                 if(rMsgMap.map[src][tag].size() >0){
                                     posted_recv=true;
                                     localbuf= (rMsgMap.map[src][tag].front()->databuf).local(); //(double*) static_cast<upcxx::global_ptr<void> > (rMsgMap.map[src][tag].front()->databuf).local(); 
                                     *(rMsgMap.map[src][tag].front()->request)= upcxx::rget(sbuf, localbuf, size);
                                     //rMsgMap.map[src][tag].pop_front();
                                     //rMsgMap.size--;
                                 }
                            } 
                            //save pull request for later when recv buffer is posted 
                            if(posted_recv==false){
                              getReq_t *req= new getReq_t(src, tag, sbuf, size);
                              pendingGetList.add(req);
                            }
                         }
                       );
                    }
                 }
            } 
	}
    } // for(t<numTasks)

    pendingGetList.process();
    upcxx:: progress();

    //!now we test if send and receive requests have been serviced

    for(int t=0; t<numCommTasks; t++){
        IcarTask* task= waitQueue.front();
        waitQueue.pop_front();
        for(inputIter it=task->input.channel.begin(); it!=task->input.channel.end(); it++){
            int src= (*it).first;
            for(std::map<int, std::deque<Message*> >::iterator it1=(*it).second.begin(); it1!=(*it).second.end(); it1++)
            {
                int tag= (*it1).first;

                if((*it1).second.size()>0)//!all messages before rear have completed
                {
                    Message *rearMessage =  (*it1).second.back();
                    if(!rearMessage->completed)
                    {
                        bool flag = false;
                        int ret_flag;
                        if(rearMessage->request->ready())
                        {
                              int dst = rearMessage->dst;
                              upcxx::rpc(graph->taskToProcess(src),
                                    [=](){
                                        sMsgMap.map[dst][tag].front()->completed=true;
                                        sMsgMap.map[dst][tag].pop_front();
                                        sMsgMap.size--;
                                    }
                              );

                            delete rearMessage->request;
                            rearMessage->completeRequest();
			    rMsgMap.map[src][tag].pop_front();
			    rMsgMap.size--;
                        }
                    }
                } 
	    }
        } 
	waitQueue.push_back(task);
    } 

    for(int t=0; t<taskList.size(); t++)
    {
        IcarTask* task= taskList[t];
        for(outputIter it=task->output.channel.begin(); it!=task->output.channel.end(); it++){
            int dst= (*it).first;
            for(std::map<int, std::deque<Message*> >::iterator it1=(*it).second.begin(); it1!=(*it).second.end(); it1++)
            {
                int tag= (*it1).first;
                if((*it1).second.size() >0) 
                {   
                    Message *frontMessage = (*it1).second.front();
                    if(frontMessage->served) //!latest receive request has NOT been completed
                    {   
                        bool flag = false;
                        int ret_flag;
                        if(frontMessage->completed)
                        {   
			    (*it1).second.pop_front();
		   	    delete frontMessage;
                        }
                    }
                } 
            }    
        }
    }

}// serviceRemoteRequests
#endif




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
#ifdef MULTI_THREADED 
#else
	    while(readyQueue.size()>0){
		IcarTask* t=readyQueue.front();
		readyQueue.pop_front();
		t->Run();
		if(t->getState()==RUNNABLE)readyQueue.push_back(t);
                else if(t->getState()==WAITING)waitQueue.push_back(t);
		     else if(t->getState()==FINISHED) finishedTasks++;
	    }
#endif



	    //check if any tasks just become runnable
	    //we take one task at a time to check dependencies, if some dependencies are still not satisfied push the task back to the wait queue
	    int numTasksToCheck= waitQueue.size();
	    for(int checkedTasks=0; checkedTasks<numTasksToCheck; checkedTasks++){
		IcarTask* t=waitQueue.front();
	        waitQueue.pop_front();
 	        if(t->checkDependencies()) readyQueue.push_back(t);
		else waitQueue.push_back(t); 
            }
#ifdef MULTI_THREADED 
#else
            serviceCommunicationRequest(graph);	    
#endif
 	    //check if all tasks have finished
	    if(finishedTasks==nTasks) break;
	}
        ///sanity check
	assert(waitQueue.size()==0);
	assert(readyQueue.size()==0);
	assert(execQueue.size()==0);
    }

    void RTS::Barrier(){
        upcxx::barrier();
    }
}

