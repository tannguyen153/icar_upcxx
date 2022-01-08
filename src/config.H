#ifndef P_RT_CONFIG_H
#define P_RT_CONFIG_H

namespace ICAR_UPCXX
{
  static const int NUM_THREAD_TEAMS=1;
  static const int NUM_THREADS_PER_TEAM=4;
  static const int NUM_COMM_THREADS=1;
  static const int NUM_PREGENERATED_PACKAGES=3;
  static const int MSG_QUEUE_DEFAULT_MAXSIZE=3; // same as num of pregenerated packages because these got swaped between pQ and recycleQ
  static const int TASK_QUEUE_DEFAULT_SIZE=512;
  static const int MAX_SQRT_TAG=512;  //sqrt(512*512)
  static const bool NUMA_AWARE_MESSAGEHANDLER=true;
  static const int LOCK_GRANULARITY=1; //!0 is queue level, 1 is region level
};
#endif
