/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   device.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:19:57 by daribeir          #+#    #+#             */
/*   Updated: 2025/04/28 01:19:58 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/


#include <linux/miscdevice.h>
#include <linux/cdev.h> /* cdev_add, cdev_init */
#include <linux/device.h>
#include <linux/fs.h> /* register_chrdev, unregister_chrdev */
#include <linux/seq_file.h> /* seq_read, seq_lseek, single_release */
#include "42kb.h"

#define READ_BUFFER_MAX 8192
DEFINE_SPINLOCK(devfile_io_spinlock);
static int ft_module_keyboard_open(struct inode *, struct file *);
static ssize_t ft_module_keyboard_read(struct file *, char *, size_t, loff_t *);
static ssize_t ft_module_keyboard_write(struct file *, const char *, size_t, loff_t *);
static struct miscdevice ft_module_keyboard_dev;
static struct file_operations ft_module_keyboard_dev_fops = {
	.read = ft_module_keyboard_read,
	.write = ft_module_keyboard_write,
	.open = ft_module_keyboard_open,
};

// file operations
static int ft_module_keyboard_open(struct inode * node, struct file * file)
{
	ft_log("Misc device opened");

	// add new test struct at tail of list

	return 0;
}

static ssize_t ft_module_keyboard_read(struct file *file, char *buff, size_t count, loff_t * offset)
{
	struct list_head *head_ptr;
	struct event_struct *entry;
	char *output_str;
	char *temp_str;
	size_t current_len = 0;
	size_t to_copy;
	int did_not_cpy;

	ft_log("Misc device read");
	
	// Allocation sécurisée
	output_str = kmalloc(READ_BUFFER_MAX, GFP_KERNEL);
	if (!output_str) {
		ft_warn("Failed to allocate read buffer");
		return -ENOMEM;
	}
	output_str[0] = '\0';

	// Protéger accès liste
	spin_lock(&devfile_io_spinlock);
	
	head_ptr = g_driver->events_head->list.next;
	while (head_ptr != &(g_driver->events_head->list) && 
	       current_len < READ_BUFFER_MAX - 256)
	{
		entry = list_entry(head_ptr, struct event_struct, list);
		temp_str = event_to_str(*entry);
		
		// Vérifier overflow AVANT concat
		size_t needed = strlen(temp_str);
		if (current_len + needed >= READ_BUFFER_MAX) {
			kfree(temp_str);
			break;
		}
		
		strncat(output_str, temp_str, READ_BUFFER_MAX - current_len - 1);
		current_len = strlen(output_str);
		kfree(temp_str);

		head_ptr = head_ptr->next;
	}
	spin_unlock(&devfile_io_spinlock);
	
	if (*offset >= current_len)
	{
		kfree(output_str);
		return 0;
	}

	size_t remaining = current_len - *offset;
	to_copy = remaining < count ? remaining : count;

	did_not_cpy = copy_to_user(buff, output_str + *offset, to_copy);
	
	if (did_not_cpy > 0) {
		ft_warn("copy_to_user failed");
		kfree(output_str);
		return -EFAULT;
	}

	*offset += (to_copy - did_not_cpy);
	
	kfree(output_str);
	return to_copy - did_not_cpy;
}


static ssize_t ft_module_keyboard_write(struct file * file, const char *buff, size_t, loff_t *offset)
{
	ft_log("Misc device written");
	return 0;
}

int ft_create_misc_device(void)
{
	ft_module_keyboard_dev.minor = MISC_DYNAMIC_MINOR;
    ft_module_keyboard_dev.name = DEV_NAME;
	ft_module_keyboard_dev.fops = &ft_module_keyboard_dev_fops;
	return misc_register(&ft_module_keyboard_dev);
}

void ft_destroy_misc_device(void)
{
	misc_deregister(&ft_module_keyboard_dev);
}