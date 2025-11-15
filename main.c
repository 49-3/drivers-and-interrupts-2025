/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:19:29 by daribeir          #+#    #+#             */
/*   Updated: 2025/11/16 00:28:25 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/hid.h>
#include <linux/irqnr.h>
#include "42kb.h"

MODULE_LICENSE(DRIVER_LICENSE);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);

drv_struct *g_driver; // declaration of global struct
struct ft_key scancode_table[MAX_SCANCODE_SIZE] = { // declaration of scancode table
	// fill array with default value
	[0x0 ... MAX_SCANCODE_SIZE - 1] = {NULL, NULL, -1, -1},

	// fill known values
	[0x29] = {"`", "~", '`', '~'},
	[0x2] = {"1", "!", '1', '!'},
	[0x3] = {"2", "@", '2', '@'},
	[0x4] = {"3", "#", '3', '#'},
	[0x5] = {"4", "$", '4', '$'},
	[0x6] = {"5", "%", '5', '%'},
	[0x7] = {"6", "^", '6', '^'},
	[0x8] = {"7", "&", '7', '&'},
	[0x9] = {"8", "*", '8', '*'},
	[0xA] = {"9", "(", '9', '('},
	[0xB] = {"0", ")", '0', ')'},
	[0xC] = {"-", "_", '-', '_'},
	[0xD] = {"=", "+", '=', '+'},
	[0xE] = {"backspace", "backspace", 8, 8},


	[0xF] = {"tab", "tab", '\t', '\t'},
	[0x10] = {"q", "Q", 'q', 'Q'},
	[0x11] = {"w", "W", 'w', 'W'},
	[0x12] = {"e", "E", 'e', 'E'},
	[0x13] = {"r", "R", 'r', 'R'},
	[0x14] = {"t", "T", 't', 'T'},
	[0x15] = {"y", "Y", 'y', 'Y'},
	[0x16] = {"u", "U", 'u', 'U'},
	[0x17] = {"i", "I", 'i', 'I'},
	[0x18] = {"o", "O", 'o', 'O'},
	[0x19] = {"p", "P", 'p', 'P'},
	[0x1A] = {"[", "{", '[', '{'},
	[0x1B] = {"]", "}", ']', '}'},
	[0x2B] = {"\\", "|", '\\', '|'},

	[0x3A] = {"caps", "caps", -1 , -1},
	[0x1E] = {"a", "A", 'a', 'A'},
	[0x1F] = {"s", "S", 's', 'S'},
	[0x20] = {"d", "D", 'd', 'D'},
	[0x21] = {"f", "F", 'f', 'F'},
	[0x22] = {"g", "G", 'g', 'G'},
	[0x23] = {"h", "H", 'h', 'H'},
	[0x24] = {"j", "J", 'j', 'J'},
	[0x25] = {"k", "K", 'k', 'K'},
	[0x26] = {"l", "L", 'l', 'L'},
	[0x1C] = {"enter", "enter", '\n', '\n'},

	[0x2A] = {"shift", "shift", -1 , -1},
	[0x2C] = {"z", "Z", 'z', 'Z'},
	[0x2D] = {"x", "X", 'x', 'X'},
	[0x2E] = {"c", "C", 'c', 'C'},
	[0x2F] = {"v", "V", 'v', 'V'},
	[0x30] = {"b", "B", 'b', 'B'},
	[0x31] = {"n", "N", 'n', 'N'},
	[0x32] = {"m", "M", 'm', 'M'},
	[0x33] = {",", "<", ',', '<'},
	[0x34] = {".", ">", '.', '>'},
	[0x35] = {"/", "?", '/', '?'},
	[0x36] = {"shift", "shift", -1 , -1},

	[0x1D] = {"ctrl", "ctrl", -1 , -1},
	[0x39] = {"space", "space", ' ', ' '},
	[0x48] = {"uparrow", "uparrow", -1, -1},
	[0x50] = {"downarrow", "downarrow", -1, -1},
	[0x4B] = {"leftarrow", "leftarrow", -1, -1},
	[0x4D] = {"rightarrow", "rightarrow", -1, -1},

	// 37 60 52 are defer signals
};

/**
 * Module Init. Registers a USB device and creates a misc device in /dev/ft_module_keyboard
*/
int init_module(void)
{
	int result ;
	event_struct* first_event;

	// create driver struct
	g_driver = ft_create_driver();
	if (!g_driver)
	{
		ft_warn("Allocation for driver struct failed");
		return 1;
	}

	// create head event
	first_event = ft_create_event(-1, -1, NULL, -1, -1);
	if (!first_event)
	{
		ft_warn("Allocation for first event failed");
		return 1;
	}
	g_driver->events_head = first_event;

	result = ft_register_usb();
	if (result)
		ft_warn("USB Registration failed");
	else
		ft_log("USB Registration OK");

	result = ft_create_misc_device();
	if (result)
		ft_warn("MiscDev Registration failed");
	else
		ft_log("MiscDev Registration OK");

	// create tmpfile
	ft_create_tmpfile();

	result = ft_register_interrupt();
	if (result)
		ft_warn("IRQ Registration failed");
	else
		ft_log("IRQ Registration OK");

	ft_log("Module initialized");
	return 0;
}

void cleanup_module(void)
{
	ft_log("Cleaning up module");

	// print everything here with stats
	ft_log_stats_to_kernel();
	ft_log_tmpfile_with_stats();

	ft_deregister_interrupt();
	ft_destroy_misc_device();
	ft_deregister_usb();
	ft_free_driver(g_driver);

	ft_log("Module cleanup complete");
}
