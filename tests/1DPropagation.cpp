#include <upcxx/upcxx.hpp>
#include<iostream>
#include"../src/RTS_impl.H"
using namespace ICAR_UPCXX;

int NTASKS=8;
class tokenTask : public IcarTask
{
    private:
  	 int leftNeighbor;
  	 int rightNeighbor;
 	 int tag, msgSize; 
	 Message *msg;

    public:
    tokenTask(){
  	 leftNeighbor = _id==0?-1:_id-1;
  	 rightNeighbor= _id==(NTASKS-1)?-1:_id+1;
 	 tag=0; msgSize=0;
	 if(_id==0) msg= new Message(); //an empty message
	 else msg=NULL;
	 //register the data dependency
	 if(leftNeighbor!=-1)  input.add(leftNeighbor, tag, msgSize);//waiting for a message from the left neighbor
    }
    ~tokenTask(){if(msg) free(msg);}
    void Run(){
	 if(_id==0){
            output.put(rightNeighbor, tag, msg);
	 }else{
	    Message *msg= input.get(leftNeighbor, tag); 
            if(rightNeighbor!=-1)output.put(rightNeighbor, tag, msg);
	 } 
	 _state=FINISHED;
    }
};

int main(int argc, char **argv){
    upcxx::init();
    ICAR_UPCXX::RTS rts; 
    rts.Init(upcxx::rank_me(), upcxx::rank_n());
    std::cout<<"Hello from Worker Thread # "<<rts.MyProc()<<std::endl;
    IcarGraph<1, tokenTask>* g= new IcarGraph<1, tokenTask>;
    g->Create1DIcarGraph(NTASKS);
    rts.Barrier();
    rts.Run((IcarGraph<1, IcarTask>*) g);
    rts.Barrier();
    delete g;
    rts.Finalize();
    upcxx::finalize();
}
