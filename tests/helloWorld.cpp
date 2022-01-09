#include <upcxx/upcxx.hpp>
#include<iostream>
#include"../src/RTS_impl.H"
using namespace ICAR_UPCXX;

int NTASKS=8;
class greetingTask : public IcarTask
{
    public:
    void initialize(){_state=RUNNABLE;}
    void Run(){
         std::cout<<"Hello from Task #"<< _id<< "(Derived from class IcarTask)"<<std::endl;
	 _state=FINISHED;
    }
};

int main(int argc, char **argv){
    upcxx::init();
    ICAR_UPCXX::RTS rts; 
    rts.Init(upcxx::rank_me(), upcxx::rank_n());
    std::cout<<"Hello from Worker Thread # "<<rts.MyProc()<<std::endl;
    IcarGraph<1, greetingTask>* g= new IcarGraph<1, greetingTask>;
    g->Create1DIcarGraph(NTASKS);
    rts.Barrier();
    rts.Run((IcarGraph<1, IcarTask>*) g);
    rts.Barrier();
    delete g;
    rts.Finalize();
    upcxx::finalize();
}
