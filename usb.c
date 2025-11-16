/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   usb.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:20:36 by daribeir          #+#    #+#             */
/*   Updated: 2025/11/16 03:12:58 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/

#include <linux/usb.h>		/* Needed for driver macros*/
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/input.h>
#include "42kb.h"

extern spinlock_t devfile_io_spinlock;

/**
 * get_key_name_from_keycode - Convert Linux keycode to key name
 */
static const char *get_key_name_from_keycode(unsigned int code)
{
	switch (code) {
		case KEY_A: return "a";
		case KEY_B: return "b";
		case KEY_C: return "c";
		case KEY_D: return "d";
		case KEY_E: return "e";
		case KEY_F: return "f";
		case KEY_G: return "g";
		case KEY_H: return "h";
		case KEY_I: return "i";
		case KEY_J: return "j";
		case KEY_K: return "k";
		case KEY_L: return "l";
		case KEY_M: return "m";
		case KEY_N: return "n";
		case KEY_O: return "o";
		case KEY_P: return "p";
		case KEY_Q: return "q";
		case KEY_R: return "r";
		case KEY_S: return "s";
		case KEY_T: return "t";
		case KEY_U: return "u";
		case KEY_V: return "v";
		case KEY_W: return "w";
		case KEY_X: return "x";
		case KEY_Y: return "y";
		case KEY_Z: return "z";
		case KEY_0: return "0";
		case KEY_1: return "1";
		case KEY_2: return "2";
		case KEY_3: return "3";
		case KEY_4: return "4";
		case KEY_5: return "5";
		case KEY_6: return "6";
		case KEY_7: return "7";
		case KEY_8: return "8";
		case KEY_9: return "9";
		case KEY_SPACE: return "space";
		case KEY_ENTER: return "return";
		case KEY_TAB: return "tab";
		case KEY_BACKSPACE: return "backspace";
		case KEY_LEFTSHIFT: return "shift";
		case KEY_RIGHTSHIFT: return "shift";
		case KEY_LEFTCTRL: return "ctrl";
		case KEY_RIGHTCTRL: return "ctrl";
		case KEY_LEFTALT: return "alt";
		case KEY_RIGHTALT: return "alt";
		case KEY_CAPSLOCK: return "caps";
		case KEY_UP: return "uparrow";
		case KEY_DOWN: return "downarrow";
		case KEY_LEFT: return "leftarrow";
		case KEY_RIGHT: return "rightarrow";
		case KEY_DELETE: return "delete";
		case KEY_HOME: return "home";
		case KEY_END: return "end";
		case KEY_PAGEUP: return "pageup";
		case KEY_PAGEDOWN: return "pagedown";
		case KEY_ESC: return "escape";
		default: return "key";
	}
}

/**
 * handle_input_event - Capture HID keyboard events
 *
 * This function intercepts keyboard events from USB HID devices
 * and logs them to our event list
 */
static void handle_input_event(struct input_handle *handle, unsigned int type,
			unsigned int code, int value)
{
	event_struct *event;
	const char *key_name;
	char ascii_val = 0;

	// Only process keyboard events (EV_KEY)
	if (type != EV_KEY)
		return;

	// Ignore non-key events
	if (code >= KEY_MAX)
		return;

	// Create event
	event = ft_create_event(code, value, "", 0, 0);
	if (!event)
		return;

	// Get key name
	key_name = get_key_name_from_keycode(code);

	// Calculate ASCII value for printable keys
	if (code >= KEY_A && code <= KEY_Z) {
		ascii_val = 'a' + (code - KEY_A);
	} else if (code >= KEY_0 && code <= KEY_9) {
		ascii_val = '0' + (code - KEY_0);
	} else {
		switch (code) {
			case KEY_SPACE: ascii_val = ' '; break;
			case KEY_ENTER: ascii_val = '\n'; break;
			case KEY_TAB: ascii_val = '\t'; break;
			case KEY_BACKSPACE: ascii_val = 8; break;
			default: ascii_val = 0;
		}
	}

	// Set event properties
	event->scan_code = code;
	event->is_pressed = (value > 0) ? 1 : 0;
	event->name = (char *)key_name;
	event->time = ktime_get_seconds();
	event->ascii_value = ascii_val;

	// Add to list with spinlock protection
	spin_lock(&devfile_io_spinlock);
	list_add_tail(&(event->list), &(g_driver->events_head->list));
	g_driver->total_events++;
	spin_unlock(&devfile_io_spinlock);

	// Log event to /tmp in real-time
	ft_log_event_to_tmpfile(event);

	// Debug: Disabled to reduce log noise
	// ft_log("USB HID Key captured");
}

static int handle_input_connect(struct input_handler *handler,
				struct input_dev *dev,
				const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "42kb";

	error = input_register_handle(handle);
	if (error)
		goto err_free_handle;

	error = input_open_device(handle);
	if (error)
		goto err_unregister_handle;

	return 0;

err_unregister_handle:
	input_unregister_handle(handle);
err_free_handle:
	kfree(handle);
	return error;
}

static void handle_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id input_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};
MODULE_DEVICE_TABLE(input, input_ids);

static struct input_handler input_handler = {
	.event = handle_input_event,
	.connect = handle_input_connect,
	.disconnect = handle_input_disconnect,
	.name = "42kb_input",
	.id_table = input_ids,
};

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
	int ret;

	// Register input handler for keyboard events
	ret = input_register_handler(&input_handler);
	if (ret) {
		ft_warn("Failed to register input handler");
		return ret;
	}

	// Register USB driver
	ret = usb_register(&input_driver);
	if (ret) {
		ft_warn("Failed to register USB driver");
		input_unregister_handler(&input_handler);
		return ret;
	}

	return 0;
}

void ft_deregister_usb(void)
{
	usb_deregister(&input_driver);
	input_unregister_handler(&input_handler);
}