#include "opt-A2.h"
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#include <mips/trapframe.h>
#include <vm.h>
#include <test.h>
#include <vfs.h>
#include <synch.h>
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

#if OPT_A2
int sys_execv(const char *program, char **args){
  if(program==NULL || args == NULL){
    return EFAULT;
  }
  struct vnode *v;
  struct addrspace *as;
  vaddr_t entrypoint, stackptr, stackptr2;
  int result;
  int counter=0;
  char *progname;
  char *prog_path;
  int i = 0;
  int len;
  
  
  // count num arguments and copy them into kerenl
  while(args[counter] != NULL){
    counter++;
  }

  char **myargs=kmalloc((counter+1)*sizeof(char *));
  myargs[counter]=NULL;
  result = copyin((const_userptr_t) args, myargs, sizeof(char *));
  if (result){
    kfree(myargs);
    return ENOMEM;
  }

  while (args[i] != NULL){
    len = strlen(args[i]+1);
    myargs[i]=kmalloc(sizeof (char)*len);
    result = copyinstr((userptr_t) args[i], myargs[i], len, NULL);
    if (result){
      kfree(myargs);
      return ENOMEM;
    }
    i++;
  }


  myargs[i]=NULL;
  // Copy the progname path into kernel
  len = strlen(program)+1;
  prog_path = kmalloc(len*sizeof(char));
  result = copyinstr ((userptr_t)program, prog_path, len, NULL);
  if (result){
    kfree(myargs);
    kfree(prog_path);
    return ENOMEM;
  }

   
  // OPen file
  progname = kstrdup(program);
  if (progname == NULL){
    return ENOMEM;
  }

  result = vfs_open(progname, O_RDONLY, 0, &v);
  if (result){
    kfree(progname);
    kfree(myargs);
    return result;
  }
  
  struct addrspace *curas = curproc_getas();
  as_destroy(curas);
  // create new address space, set process to the new address space, and active it
  as = as_create();
  if (as==NULL){
    vfs_close(v);
    return ENOMEM;
  }

  // switch to it and active it
  curproc_setas(as);
  as_activate();

  // load the excutable

  result = load_elf(v, &entrypoint);
  if (result){
    vfs_close(v);
    return result;
  }

  // Done with the file now
  vfs_close(v);

  // Define the user stack in the add space
  result = as_define_stack(as, &stackptr);
  if (result){ return result;}

  char **arg_ptr= kmalloc(sizeof(char *)*(counter+1));
  if (arg_ptr==NULL){
    return ENOMEM;
  }
  // 
  for (int i=counter-1; i>=0; i--){
    len = strlen(myargs[i])+1;
    stackptr -=len;
    arg_ptr[i] = (char *)stackptr;
    result = copyout(myargs[i], (userptr_t)stackptr, len);
    if (result){return result;}
  }
  arg_ptr[counter] = NULL;

  stackptr = ROUNDUP(stackptr, 4);
  stackptr = stackptr - (sizeof(char *)*(counter+1));
  result=copyout(arg_ptr, (userptr_t) stackptr, (counter+1)*(sizeof(char *)));
  if (result){return result;}
  stackptr2= stackptr;
  stackptr=ROUNDUP(stackptr, 8);

  // call enter _new_process with address to the arg on the stack
  enter_new_process(counter /*argc*/, (userptr_t)stackptr2, stackptr, entrypoint);

  kfree(arg_ptr);
  kfree(prog_path);
  kfree(myargs);
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
} 

//
//
//
// SYS_FORK A2a
//
//
//

int sys_fork(struct trapframe *tf, pid_t *retval){
  KASSERT(curproc != NULL);


  // create a new process using old processor name
  //  if child processor is not created by insufficitient virtual memory
  struct proc *childP = proc_create_runprogram(curproc->p_name);
  if (childP == NULL){
    return ENOMEM;
  } 

  // copy address from the current process
  struct addrspace *childas;
  int resultCopy=as_copy(curproc->p_addrspace, &childas);
  if (resultCopy || childas == NULL){
    proc_destroy(childP);
    return (ENOMEM);
  }
  spinlock_acquire(&childP->p_lock);
  childP->p_addrspace = childas;
  spinlock_release(&childP->p_lock);
  //struct proc *newproc = childP;
  //KASSERT(newproc);
  //spinlock_acquire(&newproc->p_lock);
  //newproc->p_addrspace = childas;
  //spinlock_release(&newproc->p_lock);

  // Assign PID to child process and link it to parent process parentP
  KASSERT(pidLock != NULL);
  KASSERT(procTable != NULL);
  childP->parentP = curproc;

  struct procCopy *pintable = array_get(procTable, childP->pid-2);
  lock_acquire(pintable->pLock);
  pintable->parent_pid = curproc->pid;
  lock_release(pintable->pLock);
  //lock_acquire(pintable->pLock);
  //pintable->parentP=childP->parentP;
  //lock_release(pintable->pLock);

  // create thread from parent and change to child process
  struct trapframe *childtf = kmalloc(sizeof(struct trapframe));
  if (childtf == NULL){
    proc_destroy(childP);
    return ENOMEM;
  }
  memcpy(childtf, tf, sizeof(struct trapframe));

  int thrfork = thread_fork(curproc->p_name, childP, (void *)enter_forked_process, childtf, 2);
  if (thrfork){
    proc_destroy(childP);
    kfree(childtf);
    return ENOMEM;
  }

  
  //return 0 for the child process, and the PID of the child process for the parent.
  *retval = childP->pid;
  return (0);
}

#endif /* OPT_A2 */


void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;


  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  //(void)exitcode;

#if OPT_A2

  //kprintf("sys_exit"); 
  KASSERT(procTable);
  struct procCopy *pintable = array_get(procTable, p->pid-2);
  KASSERT(p);
  KASSERT(pintable);
  if ((pintable != NULL)&&(p != NULL)){
    p->exit = true;
    p->exitStatus = _MKWAIT_EXIT(exitcode);
    p->parentP = NULL;
    pintable->exit=true;
    pintable->exitStatus= p->exitStatus;

    lock_acquire(pintable->pLock);
    cv_signal(pintable->pCV, pintable->pLock);
    lock_release(pintable->pLock);
  }
#endif /* OPT_A2 */

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


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  
  //kprintf("sys_getpid"); 
#if OPT_A2
  KASSERT(curproc != NULL);
  *retval = curproc->pid;
#else
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

#if OPT_A2
 // kprintf("sys_waitpid"); 
  struct procCopy *pintable = array_get(procTable, pid-2);
  KASSERT(pintable);
  if (pintable == NULL){
    return ESRCH;
  }
  // if pid is not a child of curproc
  if (pintable->parent_pid != curproc->pid){
    return ECHILD;
  }
  // if pid(child) is already exited
  if (pintable->exit== false){
    lock_acquire(pintable->pLock);
    cv_wait(pintable->pCV, pintable->pLock);
    lock_release(pintable->pLock);
    exitstatus = pintable->exitStatus;
  }
  else {
    exitstatus=pintable->exitStatus;
  }

#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif /* OPT_A2 */
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

