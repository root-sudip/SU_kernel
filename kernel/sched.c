#include "type.h"
#include "const.h"
#include "traps.h"
#include "irq.h"
#include "list.h"
#include "wait.h"
#include "sched.h"
#include "proc.h"
#include "asm-i386/panic.h"
#include "sched_fair.h"
#include "fork.h"
#include "printf.h"

struct task_struct *current = NULL;
/*struct task_struct *init = &proc_table[1];*/
/*struct kernel_stat kstat = { 0 };*/

struct rq sched_rq;

/*register unsigned long current_stack_pointer asm("esp") __used;*/

/*  how to get the thread information struct from C */
static inline struct thread_info *current_thread_info(void)
{
    return (struct thread_info *)(current_stack_pointer & ~(THREAD_SIZE - 1));
}

/* set process pid */
int setpid()
{
    return 0;
}

void unblock(struct task_struct *p)
{
    p->state = TASK_RUNNING;
}

void block(struct task_struct *p)
{
    p->state = TASK_INTERRUPTIBLE;
    schedule();
}

/* init the sched_rq  */
void init_sched()
{
    rr_runqueue.rr_nr_running = 0;
    INIT_LIST_HEAD(&(rr_runqueue.rr_rq_list));

    cfs_runqueue.cfs_nr_running = 0;
    cfs_runqueue.task_timeline.rb_node = NULL;
    cfs_runqueue.min_vruntime = MIN_VRUNTIME;

    init_pidmap();
}

void switch_to(PROCESS *prev,PROCESS *next)
{
}

/* sched_rr  */
static struct task_struct * rr_next_task(struct rq *rq)
{
    struct task_struct *p = NULL, *tsk = NULL;
    struct list_head *head, *pos, *n;
    int greatest_ticks = 0;	

rr_repeat:
    head = &(rq->u.rr.rr_rq_list);

    if(list_empty_careful(head))
    {
        panic("no task in running queue.");
    }

    list_for_each_safe(pos, n, head)
    {
        tsk = list_entry(pos, struct task_struct, list);
        assert(tsk->state == TASK_RUNNING);
        if(tsk->ticks > greatest_ticks)
        {
            greatest_ticks = tsk->ticks;
            p = tsk ;	
        }	
    }

    if (!greatest_ticks) 
    {
        head = &(rq->u.rr.rr_rq_list);
        list_for_each_safe(pos, n, head)
        {
            tsk = list_entry(pos, struct task_struct, list);
            tsk->ticks = tsk->priority;
        }
        goto rr_repeat;
    }

    /*disp_int(greatest_ticks);*/
    return p;
}

/* sched_rr */
static void rr_enqueue(struct rq *rq, struct task_struct *p, int wakeup,bool head)
{
    if(p)
    {
        struct list_head *head = &(rq->u.rr.rr_rq_list);
        list_add(&(p->list), head);
    }
}

/* sched_rr  */
static void rr_dequeue(struct rq *rq, struct task_struct *p, int sleep)
{
    if(p)
    {
        /*struct list_head *head = &(rq->u.rr.rr_rq_list);*/
        list_del(&(p->list));
    }
}

/* cfs enqueue  */
static void cfs_enqueue(struct rq *rq, struct task_struct *p, int wakeup,bool head)
{
    struct rb_root *root = &(CFS_RUNQUEUE(rq).task_timeline);

    /*wakeup */
    if(wakeup == 0)
    {
        p->sched_entity.vruntime += CFS_RUNQUEUE(rq).min_vruntime; 
    }

    ts_insert(root, &(p->sched_entity));
}

static void cfs_dequeue(struct rq *rq, struct task_struct *p, int sleep)
{
    struct rb_root *root = &(CFS_RUNQUEUE(rq).task_timeline);
    /*sleep long time*/
    if(sleep == 1)
    {
        p->sched_entity.vruntime -= CFS_RUNQUEUE(rq).min_vruntime; 
    }
    ts_earse(root, &(p->sched_entity));
}

static struct task_struct * cfs_next_task(struct rq *rq)
{
    struct rb_root *root = &(CFS_RUNQUEUE(rq).task_timeline);

    current->sched_entity.vruntime -= cfs_runqueue.min_vruntime;
    cfs_dequeue(rq,current,0);

    struct sched_entity *se = ts_leftmost(root);
    struct task_struct *tsk = se_entry(se, struct task_struct, sched_entity);

    CFS_RUNQUEUE(rq).min_vruntime = se->vruntime;

    if(current->state != TASK_RUNNING)
    {
    }
    else
    {
        cfs_enqueue(rq,current,0,0);
    }

    return tsk;
}

struct sched_class rr_sched = 
{
    /*.enqueue_task = rr_enqueue,*/
    /*.dequeue_task = rr_dequeue,*/
    /*.pick_next_task = rr_next_task,*/
    .enqueue_task = cfs_enqueue,
    .dequeue_task = cfs_dequeue,
    .pick_next_task = cfs_next_task,
    .switched_from = NULL,
    .switched_to = NULL,
    .prio_changed = NULL,
};

/* schedule the process  */
void schedule()
{
    disable_int();

    struct task_struct *p = NULL;
    //based on PRI
    //choose the best process
    p = current->sched_class->pick_next_task(&sched_rq); 
    /*printk("name=%s.",p->command);*/

    if(p)
    {
        if(p == current)
        {
        }
        else
        {
            current = p;
        }
    }
    else
    {
        printk("*");
    }

    enable_int();
}
