#include <linux/module.h>
#include <linux/usb.h>

// ==================== Defines
#define VENDOR_ID       0x10C4
#define PRODUCT_ID      0xEA60
#define MAX_RECV_LINE   100
#define MAX_TIMEOUT     1000

// ==================== Variables
bool ignoring = false;
int usb_max_size; 
uint usb_in, usb_out;
char *usb_in_buffer, *usb_out_buffer; 

// ==================== Functions
static void smartlamp_disconnect(struct usb_interface *ifce);
static int smartlamp_probe(struct usb_interface *ifce, const struct usb_device_id *id);
static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buff);
static ssize_t sysfs_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buff, size_t count); 

static int string_to_int(const char *string, long *value);
static int usb_send_cmd(const char *cmd, int param);
static char **extract_cmd_value(char *command);

// ==================== Structs

static struct usb_device *smartlamp_device; 

struct kobject *kobject;

// S_IRUGO -> 292 -> 100 100 100 [Todos Tem Permissão de Leitura]
// S_IWUSR -> 128 -> 010 000 000 [Dono  Tem Permissão de Escrita]
struct kobj_attribute led_attribute = __ATTR(led, S_IRUGO | S_IWUSR, sysfs_show, sysfs_store);
struct kobj_attribute ldr_attribute = __ATTR(ldr, S_IRUGO | S_IWUSR, sysfs_show, sysfs_store);

static const struct usb_device_id smartlamp_id[] = { 
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) }, 
    {} 
};

static struct usb_driver smartlamp_driver = {
    .name       = "smartlamp",
    .probe      = smartlamp_probe,
    .disconnect = smartlamp_disconnect,
    .id_table   = smartlamp_id,
};

module_usb_driver(smartlamp_driver);
MODULE_DEVICE_TABLE(usb, smartlamp_id);


