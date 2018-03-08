#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <kern/fcntl.h>
#include <synch.h>
#include <vfs.h>


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */
int sys_fork(struct trapframe *tf, pid_t *retval) {
        struct proc *newProc = proc_create_runprogram(curproc->p_name);//allocate process structure for child process
    
    
        if(newProc == NULL) {//make sure new proc is allocated
                 DEBUG(DB_SYSCALL, "cannot make  new process.\n");
                return ENPROC;
        }
    
        pid_t *temp = kmalloc(sizeof(pid_t));
        *temp = newProc->pid;
        array_add(curproc->childPids, (void *) temp,NULL);
   

	array_add(ProcessArray, newProc,NULL);
 
        as_copy(curproc_getas(), &(newProc->p_addrspace));// fetch the current addres space to the child-process.
        
        if(newProc->p_addrspace == NULL) { // make sure the address space is created.
                DEBUG(DB_SYSCALL, "fork cannott make addrspace.\n");
                proc_destroy(newProc);
                return ENOMEM;
        }

        struct trapframe *newtf = kmalloc(sizeof(struct trapframe)); // allocate memory space for trapframe.
        if (newtf == NULL){// make sure the new trapframe is created.
                DEBUG(DB_SYSCALL, "fork cannot make trapframe.\n");
                proc_destroy(newProc);
                return ENOMEM;
        }
        memcpy(newtf,tf, sizeof(struct trapframe));
        DEBUG(DB_SYSCALL, "fork cannot make new trap frame\n");



        int err = thread_fork(curthread->t_name, newProc, &enter_forked_process, newtf, 1);
        if(err) {// cannot do thread fork.
                proc_destroy(newProc);
                kfree(newtf);
                newtf = NULL;
                return err;
        }
	
        DEBUG(DB_SYSCALL, "fork successfully created\n");

	newProc->parentPid = curproc->pid;       
        *retval = newProc->pid;

    
    
        return 0;
}


void sys__exit(int exitcode,int sig) {
  

  lock_acquire(procLock);//////
	
  /*unsigned len = array_num(ProcessArray);
  struct proc *cur = NULL;
  for(unsigned i = 0; i < len; ++i){
     cur = array_get(ProcessArray, i);
     if(cur->pid == curproc->pid){
         break;
     }
     else{
          cur = NULL;
     }
   }*/

  struct proc *cur = NULL;
  cur = curproc;	
  KASSERT(cur != NULL);


   if (cur->parentPid == PROC_NO_PID){
	cur->state = PROC_EXI;
       array_add(PIDs, &cur->pid, NULL);
   }
   
   else{
	 cur->state = PROC_ZOM;
    if(sig){
     cur->exitCode = _MKWAIT_EXIT(exitcode);  
    }
    else {
      cur->exitCode = _MKWAIT_SIG(exitcode);
    }
    cv_broadcast(waitCV, procLock);		
   }
 
  for (unsigned int i = 0; i < array_num(ProcessArray); i++){

    struct proc *tempproc = array_get(ProcessArray,i);
    
    if((tempproc->parentPid == cur->pid) && (tempproc->state == PROC_ZOM)) {
      tempproc->state = PROC_EXI;
      tempproc->parentPid = PROC_NO_PID;
     array_add(PIDs, &tempproc->pid, NULL);
    }
	
  }
  lock_release(procLock);///////








   struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}

/* 

*/

/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval) // done
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval){
// kprintf("entering"); 
  int exitstatus;
  int result;
///////
 

	lock_acquire(procLock);

    if(!checkChildProc(curproc,pid)){
      result = (ECHILD);//no such child;
//   kprintf("N"); 
     }
  



  int res = 0;
  unsigned len = array_num(ProcessArray);
 // kprintf("%d\n",array_num(ProcessArray));
  struct proc *cur = NULL; 
//  cur =  does_currentProc_exists(pid);
  for(unsigned i = 0; i < len; ++i){
		
   cur = array_get(ProcessArray, i);
//		kprintf("%d\n",pid);
//		kprintf("%d\n",cur->pid);
		if(cur->pid == pid){
			kprintf("T");
			res = 1;
			break;
		}
//	cur = NULL;
  }

//kprintf("%s\n%s\n%s\n","a","b","c");	
  


if(cur == NULL || res == 0){//could not find such process.
     result = (ESRCH);
kprintf("F");
//	kprintf("no such proc"); 
  }


 
    
    //struct procTable *pt1 = getPT(pid);
    
    struct proc *parent = curproc;
    

    if(parent->pid != cur->parentPid) {
        result = ECHILD;// no such child
    }
    
    if(result){
        lock_release(procLock);// fail
        return(result);
	}
    
    if (options != 0) {
        return(EINVAL);
    }
    
    
    while(cur->state == 1) { //currently running
        cv_wait(waitCV, procLock);
    }/////
  


  
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = cur->exitCode;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
      kprintf("reslt"); 
   //*retval = -1;
    return(result);
  }
  *retval = pid;
  return(0);
}



int sys_execv(char * progname, char ** args){
    int argcounter = 0;
    
    struct addrspace *oldas;
    struct addrspace *newas;
    struct vnode *v;
    vaddr_t entrypoint;
    vaddr_t stackptr;
    int result;
    
    if(progname == NULL){
        return EFAULT;
    }
    
    size_t programLen = strlen(progname);
    programLen += 1;
    
    
    char *newProgram = kmalloc(sizeof(char *) * programLen);
    if(programLen == 0){
        return ENOMEM;
    }
    result = copyinstr((userptr_t)progname, newProgram, programLen, NULL);
    
    if(newProgram == NULL){
        return ENOMEM;
    }
    
    if(result){
        return result;
    }
    
    while(1){
        if(args[argcounter] == NULL){
            break;
        }
        
        if(strlen(args[argcounter]) > 1024){ // upper bound
            return (E2BIG);
            
        }
            argcounter += 1;
    }
    
    char ** Args = kmalloc((argcounter + 1));
    
    if(argcounter > 64) {// upper bound
        return E2BIG;
    }
    
    for (int i = 0; i <= argcounter; i++){
        if(i == argcounter){
            Args[argcounter] = NULL;
            break;
        }
        Args[i] = kmalloc((strlen(args[i]) + 1) * sizeof(char));
        result = copyinstr((userptr_t) args[i], Args[i], strlen(args[i]) + 1, NULL);
        if(result)
            return result;
    }
    // Open the file
    char *progname2;
    progname2 = kstrdup(progname);
    result = vfs_open(progname2, O_RDONLY, 0, &v);
    kfree(progname2);
    if (result) {
        return result;
    }

    newas = as_create();

    if (newas ==NULL) {
        vfs_close(v);
        return ENOMEM;
    }

    curproc_setas(newas);
    as_activate();
    
    // Load the elf
    
    result = load_elf(v, &entrypoint);
    if (result) {
        vfs_close(v);
        curproc_setas(oldas);
        return result;
    }
    
    // file done.
    vfs_close(v);
    
    

    result = as_define_stack(newas, &stackptr);
    if (result) {
        curproc_setas(oldas);
        return result;
    }
    
    

    
    vaddr_t argptr[argcounter + 1];
    
    int i = argcounter - 1;
    while(i>=0)
    {   stackptr -= strlen(Args[i]) + 1;
        result = copyoutstr(Args[i], (userptr_t)stackptr, strlen(Args[i]) + 1, NULL);
        if(result)
            return result;
        argptr[i] = stackptr;
        --i;
    }
    
    while(1){
        if((stackptr % 4) == 0){
            break;
        }
        stackptr--;
        
    }
    
    argptr[argcounter]=0;

    int j =  argcounter;
    while(j >= 0){
        stackptr -= ROUNDUP(sizeof(vaddr_t), 4);
        result = copyout(&argptr[j], (userptr_t)stackptr, sizeof(vaddr_t));
        if(result){
            return result;
        }
        
        --j;
    }
    
    as_destroy(oldas);

    enter_new_process(argcounter , (userptr_t) stackptr,stackptr, entrypoint);

    
    return EINVAL;
    
}


