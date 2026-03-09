#include <linux/module.h>   // macro MODULE_LICENSE, module_init, module_exit, ...
#include <linux/kernel.h>   // printk()
#include <linux/init.h>     // __init, __exit
#include <linux/fs.h>       // struct file, struct file_operations
#include <linux/miscdevice.h> // struct miscdevice, misc_register(), misc_deregister()
#include <linux/slab.h>     // kmalloc(), kfree()

MODULE_LICENSE("GPL");

/* ------------------------------------------------------------
 * Struttura per i dati privati di ogni apertura del device.
 *
 * Ogni volta che un processo fa open() su /dev/periodic,
 * il kernel crea una struct file per quella specifica istanza.
 * Noi allochiamo una "device_data" e la salviamo in
 * file->private_data, cosi' ogni processo ha i suoi dati
 * separati da quelli degli altri.
 *
 * Per ora contiene solo il periodo (in nanosecondi).
 * Nelle prossime versioni aggiungeremo il timer e la waitqueue.
 * ------------------------------------------------------------ */
struct device_data {
    u64 period_ns;  // periodo in nanosecondi, 0 = non ancora settato
};

/* ------------------------------------------------------------
 * open() - chiamata quando un processo apre /dev/periodic
 *
 * Alloca la struttura privata e la aggancia a file->private_data.
 * Usiamo GFP_KERNEL come flag di allocazione: significa che
 * l'allocazione puo' bloccarsi se necessario (e' il flag normale
 * da usare in contesto di processo, cioe' fuori da interrupt).
 * ------------------------------------------------------------ */
static int device_open(struct inode *inode, struct file *file)
{
    struct device_data *data;

    data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;  // se l'allocazione fallisce, torniamo errore

    data->period_ns = 0;  // periodo non ancora settato

    file->private_data = data;  // agganciamo i dati privati al file

    printk(KERN_INFO "periodic_device: open() chiamata, dati allocati\n");
    return 0;  // 0 = successo
}

/* ------------------------------------------------------------
 * release() - chiamata quando un processo chiude /dev/periodic
 *
 * Libera la memoria allocata nell'open().
 * Importante: se non lo facciamo, la memoria rimane occupata
 * per sempre (memory leak nel kernel).
 * ------------------------------------------------------------ */
static int device_release(struct inode *inode, struct file *file)
{
    struct device_data *data = file->private_data;

    kfree(data);
    file->private_data = NULL;

    printk(KERN_INFO "periodic_device: release() chiamata, dati liberati\n");
    return 0;
}

/* ------------------------------------------------------------
 * write() - per ora serve solo a settare il periodo.
 *
 * L'utente scrive un numero (il periodo in millisecondi)
 * sul device file. Noi lo leggiamo con copy_from_user(),
 * che e' la funzione corretta per copiare dati dallo
 * spazio utente allo spazio kernel (non puoi usare
 * memcpy() perche' i puntatori user-space non sono
 * direttamente accessibili dal kernel).
 *
 * Convertiamo i millisecondi in nanosecondi perche'
 * gli hrtimer lavorano con quella unita'.
 * ------------------------------------------------------------ */
static ssize_t device_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *ppos)
{
    struct device_data *data = file->private_data;
    u64 period_ms;

    // ci aspettiamo esattamente sizeof(u64) byte
    if (count != sizeof(u64))
        return -EINVAL;  // Invalid argument

    // copiamo i dati dallo spazio utente
    if (copy_from_user(&period_ms, buf, sizeof(u64)))
        return -EFAULT;  // Bad address

    // salviamo il periodo convertito in nanosecondi
    data->period_ns = period_ms * 1000000ULL;

    printk(KERN_INFO "periodic_device: periodo settato a %llu ms (%llu ns)\n",
           period_ms, data->period_ns);

    return count;  // torniamo il numero di byte "consumati"
}

/* ------------------------------------------------------------
 * read() - per ora ritorna subito -ENOSYS (not implemented).
 *
 * Nella prossima versione questa e' la funzione che blochera'
 * il processo fino alla fine del periodo corrente.
 * ------------------------------------------------------------ */
static ssize_t device_read(struct file *file, char __user *buf,
                              size_t count, loff_t *ppos)
{
    printk(KERN_INFO "periodic_device: read() chiamata (non ancora implementata)\n");
    return -ENOSYS;
}

/* ------------------------------------------------------------
 * Tabella delle file operations.
 *
 * Questa struct dice al kernel quali funzioni chiamare
 * quando qualcuno fa open/read/write/close sul nostro device.
 * I campi non specificati valgono NULL, e il kernel usera'
 * il comportamento di default (o tornera' un errore).
 * ------------------------------------------------------------ */
static const struct file_operations device_fops = {
    .owner   = THIS_MODULE,   // tiene il modulo in memoria mentre il device e' aperto
    .open    = device_open,
    .release = device_release,
    .write   = device_write,
    .read    = device_read,
};

/* ------------------------------------------------------------
 * Struttura del misc device.
 *
 * MISC_DYNAMIC_MINOR chiede al kernel di assegnare
 * automaticamente un minor number libero, cosi' non
 * dobbiamo preoccuparci di scegliere un numero non
 * gia' usato da un altro device.
 *
 * Il campo "name" determina il nome del file in /dev/.
 * ------------------------------------------------------------ */
static struct miscdevice device_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "periodic",
    .fops  = &device_fops,
};

/* ------------------------------------------------------------
 * Funzione di init - chiamata quando il modulo viene inserito
 * con insmod / modprobe.
 * ------------------------------------------------------------ */
static int __init device_init(void)
{
    int ret;

    ret = misc_register(&device_misc);
    if (ret) {
        printk(KERN_ERR "periodic_device: errore nella registrazione: %d\n", ret);
        return ret;
    }

    printk(KERN_INFO "periodic_device: modulo caricato, device /dev/periodic creato\n");
    return 0;
}

/* ------------------------------------------------------------
 * Funzione di cleanup - chiamata quando il modulo viene
 * rimosso con rmmod.
 * ------------------------------------------------------------ */
static void __exit device_exit(void)
{
    misc_deregister(&device_misc);
    printk(KERN_INFO "periodic_device: modulo rimosso, device /dev/periodic eliminato\n");
}

module_init(device_init);
module_exit(device_exit);