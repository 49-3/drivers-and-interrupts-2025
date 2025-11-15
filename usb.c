/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   usb.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:20:36 by daribeir          #+#    #+#             */
/*   Updated: 2025/04/28 01:20:37 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/

#include <linux/usb.h>		/* Needed for driver macros*/
#include <linux/usb/input.h>
#include <linux/hid.h>
#include "42kb.h"

// function to handle probe
int handle_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	return 0;
}

// function to handle disconnect
void handle_disconnect(struct usb_interface *intf)
{
	printk(KERN_INFO "Usb DCED !\n");
}

/*
 * definition_table - hardware definitions for mouse and keyboard
 */
static struct usb_device_id definition_table[] = {
	{USB_INTERFACE_INFO(
		USB_INTERFACE_CLASS_HID,
		USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE
	)},
	{USB_INTERFACE_INFO(
		USB_INTERFACE_CLASS_HID,
		USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_KEYBOARD
	)},
	{0}
};
MODULE_DEVICE_TABLE(usb, definition_table);

// register device
static struct usb_driver input_driver = {
 .name = DRV_NAME,
 .id_table = definition_table,
 .probe = handle_probe,
 .disconnect = handle_disconnect,
};

int ft_register_usb(void)
{
	return usb_register(&input_driver);
}

void ft_deregister_usb(void)
{
	usb_deregister(&input_driver);
}