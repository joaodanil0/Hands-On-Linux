#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
 
#define MIN(a,b)        (((a) <= (b)) ? (a) : (b))
#define BULK_EP_OUT     0x01
#define BULK_EP_IN      0x82
#define MAX_PKT_SIZE    512
#define VENDOR_ID       0x10C4 
#define PRODUCT_ID      0xEA60 
 
static struct usb_device *device;
static struct usb_class_driver class;


static uint usb_in, usb_out; 
static char *usb_in_buffer, *usb_out_buffer;       // Buffers de entrada e saída da USB
bool ignore = true;
static int usb_max_size;                           // Tamanho máximo de uma mensagem USB

static int pen_open(struct inode *i, struct file *f)
{
    return 0;
}
static int pen_close(struct inode *i, struct file *f)
{
    return 0;
}
static ssize_t pen_read(struct file *f, char __user *buf, size_t cnt, loff_t *off)
{
    int retval;
    int read_cnt;
    char *bulk_buf = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);
    /* Read the data from the bulk endpoint */
    retval = usb_bulk_msg(device, usb_rcvbulkpipe(device, usb_in), bulk_buf, MAX_PKT_SIZE, &read_cnt, 1000);
    if (retval)
    {
        printk(KERN_ERR "Bulk message returned %d\n", retval);
        return retval;
    }
    if (copy_to_user(buf, bulk_buf, MIN(cnt, read_cnt)))
    {
        return -EFAULT;
    }
    kfree(bulk_buf);
    return MIN(cnt, read_cnt);
}
static ssize_t pen_write(struct file *f, const char __user *buf, size_t cnt, loff_t *off)
{   
    int retval;
    int wrote_cnt = MIN(cnt, MAX_PKT_SIZE);
    char *bulk_buf = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);
    // memset(usb_out_buffer, 0, usb_max_size);
    if (copy_from_user(bulk_buf, buf, MIN(cnt, MAX_PKT_SIZE)))
    {
        return -EFAULT;
    }
 
    /* Write the data into the bulk endpoint */
    retval = usb_bulk_msg(device, usb_sndbulkpipe(device, BULK_EP_OUT), bulk_buf, MIN(cnt, MAX_PKT_SIZE), &wrote_cnt, 5000);
    // retval = usb_bulk_msg(device, usb_sndbulkpipe(device, usb_out), usb_out_buffer, MIN(cnt, MAX_PKT_SIZE), &wrote_cnt, 5000);
    if (retval)
    {
        printk(KERN_ERR "Bulk message returned %d\n", retval);
        return retval;
    }
    
    printk(KERN_INFO "Writing %s ... [%d][%d][%d]\n", bulk_buf, wrote_cnt, usb_max_size, MAX_PKT_SIZE);

    // ========
    memset(bulk_buf, 0, MAX_PKT_SIZE);
    int read_cnt;
    char *string = kmalloc(MAX_PKT_SIZE, GFP_KERNEL);
    /* Read the data from the bulk endpoint */
    while(1){
        mdelay(100);
        retval = usb_bulk_msg(device, usb_rcvbulkpipe(device, usb_in), bulk_buf, MAX_PKT_SIZE, &read_cnt, 1000);
        if(retval == -110){
            printk(KERN_INFO "read timeout");
            break;
        }
        else if (retval)
        {
            printk(KERN_ERR "Bulk message returned %d\n", retval);
            break;
        }
        // printk(KERN_INFO "Reading %s ... [%d]\n", bulk_buf, read_cnt);
        strcat(string, bulk_buf);
    }
    
    printk(KERN_INFO "Read %s ... [%ld]\n", string, strlen(string));
    

    kfree(bulk_buf);
    return wrote_cnt;
}
 
static struct file_operations fops =
{
    .open = pen_open,
    .release = pen_close,
    .read = pen_read,
    .write = pen_write,
};
 
static int pen_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_endpoint_descriptor *usb_endpoint_in, *usb_endpoint_out;
    int retval;
    

    device = interface_to_usbdev(interface);

    ignore =  usb_find_common_endpoints(interface->cur_altsetting, &usb_endpoint_in, &usb_endpoint_out, NULL, NULL);  // AQUI
    usb_in = usb_endpoint_in->bEndpointAddress;
    usb_out = usb_endpoint_out->bEndpointAddress;
    usb_max_size = usb_endpoint_maxp(usb_endpoint_in);
    usb_in_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    usb_out_buffer = kmalloc(usb_max_size, GFP_KERNEL);

    
    class.name = "usb/pen%d";
    class.fops = &fops;
    if ((retval = usb_register_dev(interface, &class)) < 0)
    {
        /* Something prevented us from registering this driver */
        printk(KERN_ERR "Not able to get a minor for this device.");
    }
    else
    {
        printk(KERN_INFO "Minor obtained: %d\n", interface->minor);
    }
 
    return retval;
}
 
static void pen_disconnect(struct usb_interface *interface)
{
    usb_deregister_dev(interface, &class);
    kfree(usb_in_buffer);                   // Desaloca buffers
    kfree(usb_out_buffer);
}
 
/* Table of devices that work with this driver */
static struct usb_device_id pen_table[] =
{
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, pen_table);
 
static struct usb_driver pen_driver =
{
    .name = "pen_driver",
    .probe = pen_probe,
    .disconnect = pen_disconnect,
    .id_table = pen_table,
};
 
static int __init pen_init(void)
{
    int result;
 
    /* Register this driver with the USB subsystem */
    if ((result = usb_register(&pen_driver)))
    {
        printk(KERN_ERR "usb_register failed. Error number %d", result);
    }
    return result;
}
 
static void __exit pen_exit(void)
{
    /* Deregister this driver with the USB subsystem */
    usb_deregister(&pen_driver);
}
 
module_init(pen_init);
module_exit(pen_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anil Kumar Pugalia <email_at_sarika-pugs_dot_com>");
MODULE_DESCRIPTION("USB Pen Device Driver");