#include <linux/poll.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>


MODULE_AUTHOR("Salvatore Mastrangelo");
MODULE_DESCRIPTION("Test for misc devices");
MODULE_LICENSE("GPL");

static char *my_pointer;
static int my_len;

static int my_open(struct inode *inode, struct file *file)
{
  int *count;

  count = kmalloc(sizeof(int), GFP_USER);
  if (count == NULL) {
    return -1;
  }

  *count = 0;
  file->private_data = count;

  printk("Device open!\n");

  return 0;
}

static int my_close(struct inode *inode, struct file *file)
{
  kfree(file->private_data);

  return 0;
}

static ssize_t my_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
  int res, err, *count;

  count = file->private_data;
  *count = *count + 1;

  if (len > my_len) {
    res = my_len;
  } else {
    res = len;
  }
  if (my_pointer == NULL) {
    return 0;
  }
  err = copy_to_user(buf, my_pointer, res);
  if (err) {
    return -EFAULT;
  }

  if (*count == 3) {
    kfree(my_pointer);
    my_pointer = NULL;
  }

  return res;
}

static ssize_t my_write(struct file *file, const char __user * buf, size_t count, loff_t *ppos)
{
  int err;

  if (my_pointer) {
    return -1;
  }
  my_pointer = kmalloc(count, GFP_USER);
  if (my_pointer == NULL) {
    return -1;
  }
  my_len = count;

  err = copy_from_user(my_pointer, buf, count);
  if (err) {
    return -EFAULT;
  }

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
  .name = "test", 
  .fops = &my_fops
};


static int testmodule_init(void)
{
  int res;

  res = misc_register(&test_device);

  printk("Misc Register returned %d\n", res);

  return 0;
}

static void testmodule_exit(void)
{
  misc_deregister(&test_device);
}

module_init(testmodule_init);
module_exit(testmodule_exit);