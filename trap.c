#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}


void default_handler(int signum) {
  struct proc *p = myproc();
  cprintf("\nA signal %d was accepted by process %d", signum, p->pid);
}

void 
check_pending() 
{
  struct proc *p = myproc();

  if ( p == 0)		//Check process created
    return;
  if (p->signal_executing == 1)		// Check if handler already in middle of execution
    return;
  if (p->pending == 0)		// Check if there are pending signals at all
    return;
  p->signal_executing = 1; 

  for (int i = 0; i < NUMSIG; i++) {	//Signals are from 0 to 31
	   if ( (p->pending & (1 << i)) > 0) {
	      cprintf("\nFound new signal to handle!");
	      //cprintf("\nPending: %d", p->pending); 
       	      cprintf("\nThe signal received is %d", i);
	      

	     if (p->sighandlers[i] == (sighandler_t)default_handler) {	//Check if default handler
	        default_handler(i);
	     }
	     else {
		  memmove(&p->tempTf, p->tf, sizeof(struct trapframe));  // Save temporary trap frame while handler executes

		  p->tf->esp -= (uint)&sigret_end - (uint)&sigret;	// Save room for sigret assembly code
		  memmove((void*)p->tf->esp, sigret, (uint)&sigret_end - (uint)&sigret);	// Insert invocation of sigret
		  *((uint*)(p->tf->esp - 4)) = p->tf->eip;
		  *((uint*)(p->tf->esp - 8)) = i; // push i as argument signum for signal handler
		  *((uint*)(p->tf->esp - 12)) = p->tf->esp; // sigreturn system call code address
		  //*((uint*)(p->tf->esp - 12)) = p->sigreturn_address;
		  p->tf->esp -= 12;
		  p->tf->eip = (uint)(p->sighandlers[i]); // trapret will resume into signal handler

		  p->sighandlers[i] = (sighandler_t)default_handler;	// Make handler back to default handler
	     }
             p->pending = p->pending  -  ( 1 << i) ;	// Update pending - we took care of the signal
	  
	  }
	  if (p->pending <= 0)		// Check if there are pending signals at all
    		return;
	  
     }
}



