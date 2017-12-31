
#include "types.h"
#include "stat.h"
#include "user.h"

//Default handler for signal number i
void a_handler(int signum) {
  printf(1,"\nA signal %d was accepted by process %d and this is not the default handler!!!\n ", signum, getpid());
}

int main(int argc, char *argv[]) {

   sighandler_t test_handler = (sighandler_t)a_handler;
   sighandler_t prev_handler = signal(4, test_handler);   
   prev_handler += 0;

   sigsend(getpid(), 4);  
   sigsend(getpid(), 5);    
   printf(1,"\nFinished test\n");

   exit();
}
