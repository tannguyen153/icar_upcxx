#include <upcxx/upcxx.hpp>
#include<iostream>
#include"../../src/RTS_impl.H"
using namespace ICAR_UPCXX;

extern "C" {
void accum(int*, int*, int*, int*);
}

#define NTASKS 8
#define N 128
class redTask : public IcarTask
{
    private:
 	 int data[N/NTASKS], i0,i1, s;
         int leftNeighbor;
         int rightNeighbor;
         int tag, msgSize;
         Message *msg;

    public:
    void initialize(){
 	 for(int i=0; i<N/NTASKS; i++) data[i]=_id*(N/NTASKS)+i;
 	 i0=0; i1=N/NTASKS-1;
         leftNeighbor = _id==0?-1:_id-1;
         rightNeighbor= _id==(NTASKS-1)?-1:_id+1;
         tag=0; msgSize=sizeof(int);
         if(rightNeighbor){
             msg= new Message(msgSize); //an empty message
         }else msg=NULL;
	 
         if(_id==(NTASKS-1)) _state=RUNNABLE;
         else {
	    _state=WAITING;
             //register the data dependency
            add_input(rightNeighbor, tag, msgSize);//waiting for a message from the left neighbor
	 }
	 //compute partial sum
 	 accum(data, &i0, &i1, &s);
    }
    ~redTask(){if(msg) free(msg);}
    void Run(){
         if(rightNeighbor!=-1){
            Message *msg1= input.get(rightNeighbor, tag);
	    int s1= *( (int*) msg1->databuf.local());
	    s+= s1;
	 }
	 
         if(leftNeighbor!=-1){
	    memcpy((void*)msg->databuf.local(), &s, sizeof(int));
            put(leftNeighbor, tag, msg);
         }
	 if(_id==0) std::cout<<"Total Sum (graph version): "<<s<<std::endl;
         _state=FINISHED;
    }
};


int main(int argc, char** argv){
    upcxx::init();
    ICAR_UPCXX::RTS rts;
    rts.Init(upcxx::rank_me(), upcxx::rank_n());
    std::cout<<"Hello from Worker Thread # "<<rts.MyProc()<<std::endl;
    IcarGraph<1, redTask>* g= new IcarGraph<1, redTask>;
    g->Create1DIcarGraph(NTASKS);
    rts.Barrier();
    rts.Run((IcarGraph<1, IcarTask>*) g);
    rts.Barrier();

//verification
    int data[N];
    int i0=0, i1=N-1;
    int s;
    if(upcxx::rank_me()==0){
 	for(int i=0; i<N; i++) data[i]=i;
        accum(data, &i0, &i1, &s);
        std::cout<<"Total Sum (serial version): "<<s<<std::endl;
    }
    delete g;
    rts.Finalize();
    upcxx::finalize();
}
