#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/list.h>


MODULE_AUTHOR("Salvatore Mastrangelo");
MODULE_DESCRIPTION("Test for misc devices");
MODULE_LICENSE("GPL");


struct timer_task {
  struct timer_list timer;
  struct task_struct *thread;
};

struct tid_period {
  struct list_head node;
  pid_t tid;
  __u64 period_ns;
  struct timer_task *timer_task;
};

struct file_data {
  struct list_head periods_list;
  struct mutex lock;
};


static void timer_handler(struct timer_list *t)
{
  struct timer_task *tt = container_of(t, struct timer_task, timer);
  printk("Timer expired, waking up task %d\n", tt->thread->pid);

  wake_up_process(tt->thread);
}

static int my_open(struct inode *inode, struct file *file)
{
  struct file_data *data;
  data = kmalloc(sizeof(struct file_data), GFP_KERNEL);
  if (data == NULL) {
    return -ENOMEM;
  }
  INIT_LIST_HEAD(&data->periods_list);
  mutex_init(&data->lock);
  file->private_data = data;

  printk("Device open!\n");

  return 0;
}

static int my_close(struct inode *inode, struct file *file)
{
  struct file_data *data = file->private_data;
  struct list_head *periods_list = &data->periods_list;
  struct mutex *lock = &data->lock;
  struct tid_period *entry, *tmp;

  mutex_lock(lock);
  list_for_each_entry_safe(entry, tmp, periods_list, node) {
    if (entry->timer_task) {
      wake_up_process(entry->timer_task->thread);
      timer_delete_sync(&entry->timer_task->timer);
      kfree(entry->timer_task);
    }
    list_del(&entry->node);
    kfree(entry);
  }

  mutex_unlock(lock);
  kfree(file->private_data);
  printk("Device close!\n");

  return 0;
}

static ssize_t my_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
  struct file_data *data = file->private_data;
  struct list_head *periods_list = &data->periods_list;
  struct tid_period *entry;
  __u64 period_ms = 0;

  mutex_lock(&data->lock);
  __u64 start_time = jiffies;
  __u64 end_time;

  // iterate through the list to find the period for the current thread
  list_for_each_entry(entry, periods_list, node) {
    if (entry->tid == current->pid) {
      period_ms = entry->period_ns / 1000000ULL;
      break;
    }
  }

  if (!period_ms) {
    printk("No period found for thread %d\n", current->pid);
    mutex_unlock(&data->lock);
    return -EINVAL;
  }

  end_time = start_time + msecs_to_jiffies(period_ms);
  
  // // if the period has already expired, return immediately
  // if (jiffies >= end_time) {
  //   printk("Period already expired for thread %d\n", current->pid);
  //   mutex_unlock(&data->lock);
  //   return -EAGAIN;
  // }

  // if no timer task exists for this thread, create one
  if (!entry->timer_task) {
    entry->timer_task = kmalloc(sizeof(struct timer_task), GFP_KERNEL);
    if (!entry->timer_task) {
      mutex_unlock(&data->lock);
      return -ENOMEM;
    }
  }

  timer_setup(&entry->timer_task->timer, timer_handler, 0);
  entry->timer_task->thread = current;
  entry->timer_task->timer.expires = end_time;

  set_current_state(TASK_INTERRUPTIBLE);
  add_timer(&entry->timer_task->timer);
  mutex_unlock(&data->lock);

  schedule();

  mutex_lock(&data->lock);
  bool found = false;
  list_for_each_entry(entry, periods_list, node) {
    if (entry->tid == current->pid) {
      found = true;
      break;
    }
  }
  if (!found) {
    printk("Thread %d not found in list after waking up\n", current->pid);
    mutex_unlock(&data->lock);
    return -EINVAL;
  }
  if (entry->timer_task) {
    timer_delete_sync(&entry->timer_task->timer);
    kfree(entry->timer_task);
    entry->timer_task = NULL;
  }
  mutex_unlock(&data->lock);

  return len;
}

static ssize_t my_write(struct file *file, const char __user * buf, size_t count, loff_t *ppos)
{
  int err;
  __u64 period_ms;
  struct file_data *data = file->private_data;
  struct list_head *periods_list = &data->periods_list;
  struct tid_period *entry;

  if (count != sizeof(__u64)) {
    return -EINVAL;
  }

  mutex_lock(&data->lock);
  err = copy_from_user(&period_ms, buf, count);
  if (err) {
    mutex_unlock(&data->lock);
    return -EFAULT;
  }

  // check if the current thread already has a period set
  list_for_each_entry(entry, periods_list, node) {
    if (entry->tid == current->pid) {
      entry->period_ns = period_ms * 1000000ULL;
      mutex_unlock(&data->lock);
      return count;
    }
  }

  // if not found, create a new entry
  entry = kmalloc(sizeof(struct tid_period), GFP_KERNEL);
  if (!entry) {
    mutex_unlock(&data->lock);
    return -ENOMEM;
  }
  entry->tid = current->pid;
  entry->period_ns = period_ms * 1000000ULL;
  entry->timer_task = NULL;
  list_add_tail(&entry->node, periods_list);

  mutex_unlock(&data->lock);
  return count;
}

static struct file_operations my_fops = {
  .owner =        THIS_MODULE,
  .read =         my_read,
  .open =         my_open,
  .release =      my_close,
  .write =        my_write,
#if 0
  .poll =         my_poll,
  .fasync =       my_fasync,
#endif
};

static struct miscdevice test_device = {
  .minor = MISC_DYNAMIC_MINOR, 
  .name = "periodic_device", 
  .fops = &my_fops
};


static int __init testmodule_init(void)
{
  int res;

  res = misc_register(&test_device);

  printk("Misc Register returned %d\n", res);

  return res;
}

static void __exit testmodule_exit(void)
{
  misc_deregister(&test_device);
}

module_init(testmodule_init);
module_exit(testmodule_exit);