#include "smartlamp.h"

static int usb_send_cmd(const char *cmd, int param)
{
    char *BUFFER = kmalloc(MAX_RECV_LINE, GFP_KERNEL);
    long value = -1;
    int ret = -1, actual_size;
    
    pr_info("SmartLamp: Sending Command: %s\n", cmd);


    if(param == -1)
        sprintf(usb_out_buffer, "%s", cmd);
    else
        sprintf(usb_out_buffer, "%s %d", cmd, param);
    
    ret = usb_bulk_msg(smartlamp_device, usb_sndbulkpipe(smartlamp_device, usb_out), usb_out_buffer, min(strlen(usb_out_buffer), MAX_RECV_LINE), &actual_size, MAX_TIMEOUT);

    if (ret) {
        pr_err("SmartLamp: Error to send command (code: %d)", ret);
        return -1;
    }

    while(true)
    {
        mdelay(100);
        memset(usb_in_buffer, 0, MAX_RECV_LINE);
        ret = usb_bulk_msg(smartlamp_device, usb_rcvbulkpipe(smartlamp_device, usb_in), usb_in_buffer, MAX_RECV_LINE, &actual_size, MAX_TIMEOUT);
        if(ret == -110){
            pr_info("SmartLamp: Read Timeout\n");
            break;
        }
        else if (ret)
        {
            pr_err("SmartLamp: Bulk message returned %d\n", ret);
            break;
        }

        strcat(BUFFER, usb_in_buffer);
    }

    strcat(BUFFER, "\0");

    char **cmd_value = extract_cmd_value(BUFFER);

    pr_info("SmartLamp: CMD: %s |  Val: %s\n", cmd_value[0], cmd_value[1]);

    string_to_int(cmd_value[1], &value);

    kfree(cmd_value);
    kfree(BUFFER);

    return value;
}

static char **extract_cmd_value(char *command) {

    int counter = 0, i = 0, j = 0;
    char **cmd_value = kmalloc(2, GFP_KERNEL);
    char *value = kmalloc(20, GFP_KERNEL);
    char *cmd = kmalloc(20, GFP_KERNEL);

    for(i = 0; i < strlen(command); i++){

        if(command[i] == ' ') counter++;
        
        if(counter == 2){
            cmd[i] = '\0';
            i++;
            break;
        }

        cmd[i] = command[i];
    }
        
    for(; i < strlen(command); i++) {

        if(command[i] == '\r') break;
        
        value[j] = command[i];
        j++;
    }

    value[j] = '\0';

    cmd_value[0] = cmd;
    cmd_value[1] = value;

    return cmd_value;
}

static int string_to_int(const char *string, long *value)
{
    return kstrtol(string, 10, value);
}

static ssize_t sysfs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buff)
{
    int value;
    pr_info("SmartLamp: Reading: %s\n", attr->attr.name);


    if(strcmp(attr->attr.name, "led") == 0)
    {
        value = usb_send_cmd("GET_LED", -1);
    }
    else if(strcmp(attr->attr.name, "ldr") == 0)
    {
        value = usb_send_cmd("GET_LDR", -1);
    }

    sprintf(buff, "%d\n", value); 
    return strlen(buff);;
}

static ssize_t sysfs_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buff, size_t count)
{
    int ret = -2;
    long value = -1;

    ret = string_to_int(buff, &value);

    if (ret < 0) {
        pr_err("SmartLamp: Error to convert string to int.\n");
        return -EACCES;
    }

    pr_info("SmartLamp: Setting %s to %ld.\n", attr->attr.name, value);

    ret = usb_send_cmd("SET_LED ", value);

    if (ret < 0) {
        pr_err("SmartLamp: Error to setting the value\n");
        return -EACCES;
    }

    return strlen(buff);
}

static int smartlamp_probe(struct usb_interface *ifce, const struct usb_device_id *id)
{
    pr_info("SmartLamp: Device connected.");
    struct usb_endpoint_descriptor *usb_endpoint_in, *usb_endpoint_out;

    kobject = kobject_create_and_add("smartlamp", kernel_kobj);

    if(sysfs_create_file(kobject, &led_attribute.attr)){
        pr_err("LED Sysfs not created\n");
        return -1;
    }
    if(sysfs_create_file(kobject, &ldr_attribute.attr)){
        pr_err("LDR Sysfs not created\n");
        return -1;
    }

    smartlamp_device = interface_to_usbdev(ifce);
    ignoring = usb_find_common_endpoints(ifce->cur_altsetting, &usb_endpoint_in, &usb_endpoint_out, NULL, NULL);
    usb_max_size = usb_endpoint_maxp(usb_endpoint_in);
    usb_in = usb_endpoint_in->bEndpointAddress;
    usb_out = usb_endpoint_out->bEndpointAddress;

    usb_in_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    usb_out_buffer = kmalloc(usb_max_size, GFP_KERNEL);

    pr_info("SmartLamp: Device probed successfully.");
    

    return 0;
}

static void smartlamp_disconnect(struct usb_interface *ifce)
{
    kobject_put(kobject);
    sysfs_remove_file(kernel_kobj, &led_attribute.attr);
    sysfs_remove_file(kernel_kobj, &ldr_attribute.attr);
    kfree(usb_in_buffer);
    kfree(usb_out_buffer);
    pr_info("SmartLamp: Device Disconnected.");
}

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_AUTHOR("João Danilo <joaodanilo1992@gmail.com>");
MODULE_DESCRIPTION("SmartLamp device from Devtitans coure");