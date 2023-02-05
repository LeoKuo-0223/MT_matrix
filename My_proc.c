#include <linux/init.h> 
#include <linux/kernel.h> 
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/stat.h> 
#include <linux/uaccess.h> 
#include <linux/version.h>
#include <linux/string.h>
#include <linux/pid.h>


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) 
#include <linux/minmax.h> 
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#define PROCFS_MAX_SIZE 2048
#define TMP_BUFFER_SIZE 32
#define PROCFS_NAME "thread_info"


static struct proc_dir_entry *our_proc_file;
static char procfs_buffer[PROCFS_MAX_SIZE];
static char tmp_buffer[TMP_BUFFER_SIZE];
static unsigned long procfs_buffer_size = 0;
static struct task_struct *task;

static ssize_t procfile_read(struct file *filePointer, char __user *buffer,
                             size_t buffer_length, loff_t *offset)
{
    // int len = sizeof(procfs_buffer);
    int len = strlen(procfs_buffer);
    ssize_t ret = len;
 
    if (*offset >= len || copy_to_user(buffer, procfs_buffer, len)){
        pr_info("copy_to_user failed\n");
        ret = 0;
    } else {
        pr_info("procfile read %s \n", filePointer->f_path.dentry->d_name.name);
        *offset += len;
    }

    return ret;
}


int strToInteger(char *str){
    char *after;
    unsigned long res = simple_strtoul(str, &after, 10);
    return (int)res;
}


static ssize_t procfile_write(struct file *file, const char __user *buff, 
                             size_t len, loff_t *off)
{
    int tid;
    pr_info("writing\n");
    procfs_buffer_size = len;
    if (procfs_buffer_size > TMP_BUFFER_SIZE)
        procfs_buffer_size = TMP_BUFFER_SIZE;

    if (copy_from_user(tmp_buffer, buff, procfs_buffer_size)){
        pr_info("write fail\n");
        return -EFAULT; 
    }
 
    tmp_buffer[procfs_buffer_size & (TMP_BUFFER_SIZE - 1)] = '\0';
    
    pr_info("procfile write %s\n", tmp_buffer);
    strcat(procfs_buffer, tmp_buffer); //threadID
    strcat(procfs_buffer, " ");
    tid = strToInteger(tmp_buffer);
    task=pid_task(find_get_pid(tid), PIDTYPE_PID);
    if(task == NULL){
       pr_info("tid : %d not found\n", tid); 
    }else{
        unsigned long long time = task->utime / 1000000;
        unsigned long context_switch = task->nvcsw + task->nivcsw;
        // unsigned long long elapsed_time;
        memset(tmp_buffer, '\0', sizeof(tmp_buffer));
        snprintf(tmp_buffer, sizeof(tmp_buffer), "%llu", time);
        strcat(procfs_buffer, tmp_buffer); //time
        strcat(procfs_buffer, " ");
        memset(tmp_buffer, '\0', sizeof(tmp_buffer));
        snprintf(tmp_buffer, sizeof(tmp_buffer), "%lu", context_switch);
        strcat(procfs_buffer, tmp_buffer); //context switch
        strcat(procfs_buffer, "\n");
    }

    *off += procfs_buffer_size;
    return procfs_buffer_size; 
} 

#ifdef HAVE_PROC_OPS 
static const struct proc_ops proc_file_fops = { 
    .proc_read = procfile_read, 
    .proc_write = procfile_write, 
}; 
#else 
static const struct file_operations proc_file_fops = { 
    .read = procfile_read, 
    .write = procfile_write, 
}; 
#endif


//module init
static int __init myproc_init(void) 
{ 
    our_proc_file = proc_create(PROCFS_NAME, 0777, NULL, &proc_file_fops); 
    if (NULL == our_proc_file) {
        proc_remove(our_proc_file);
        pr_alert("Error:Could not initialize /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }
    procfs_buffer[0]='\0';
    pr_info("/proc/%s created\n", PROCFS_NAME); 
    return 0;
} 


//module exit
static void __exit myproc_exit(void) 
{ 
    proc_remove(our_proc_file);
    pr_info("/proc/%s removed\n", PROCFS_NAME);
} 
 
module_init(myproc_init); 
module_exit(myproc_exit);

MODULE_LICENSE("GPL"); 