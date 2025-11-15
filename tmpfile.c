/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tmpfile.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:20:17 by daribeir          #+#    #+#             */
/*   Updated: 2025/04/28 01:20:18 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/


#include "42kb.h"
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/string.h>

struct file* tmpfile = 0;
loff_t tmpoffset = 0;

static int ft_write_tmpfile(char *str, int len)
{
	const struct file_operations	*f_op = tmpfile->f_op;

	if (!f_op)
	{
		ft_warn("No fop on tmpfile\n");
		return 1;
	}

	return kernel_write(tmpfile, str, len, &tmpoffset);
}

int ft_create_tmpfile(void)
{
	char *time;
	char *filename;
	char *prefix;

	time = ft_itoa(ktime_get_seconds());
	filename = kmalloc(69, GFP_KERNEL);
	prefix = kmalloc(69, GFP_KERNEL);
	strcpy(prefix, "/tmp/42kb");
	filename = strcat(prefix, time);
	tmpfile = filp_open(filename, O_WRONLY | O_CREAT, S_IRWXU);
	kfree(filename);
	kfree(time);
	if (tmpfile) {
		printk("Created tmpfile %s\n", tmpfile->f_path.dentry->d_name.name);
		return 0;
	}
	else
	{
		ft_warn("Cannot create tmpfile");
		return 1;
	}
}

/**
 * ft_log_tmpfile_with_stats - Write kernel log to /tmp with statistics
 */
void ft_log_tmpfile_with_stats(void)
{
	struct list_head *head_ptr;
	struct event_struct *entry;
	char *output_str;
	char *stats_str;
	int total_events = 0;
	int total_alpha = 0;
	int count = 0;

	output_str = kmalloc(2048, GFP_KERNEL);
	stats_str = kmalloc(512, GFP_KERNEL);
	if (!output_str || !stats_str) {
		ft_warn("Memory allocation failed for stats");
		if (output_str) kfree(output_str);
		if (stats_str) kfree(stats_str);
		return;
	}

	spin_lock(&devfile_io_spinlock);
	head_ptr = g_driver->events_head->list.next;
	while (head_ptr != &(g_driver->events_head->list)) {
		entry = list_entry(head_ptr, struct event_struct, list);
		total_events++;
		
		if (entry->is_pressed && 
		    entry->ascii_value > 32 && 
		    entry->ascii_value < 127) {
			total_alpha++;
		}
		head_ptr = head_ptr->next;
	}

	snprintf(stats_str, 512,
		"=== KEYBOARD LOG ===\n"
		"Total Events: %d\n"
		"Alphanumeric Keys: %d\n"
		"=== EVENTS ===\n",
		total_events, total_alpha);

	ft_write_tmpfile(stats_str, strlen(stats_str));

	count = 0;
	head_ptr = g_driver->events_head->list.next;
	while (head_ptr != &(g_driver->events_head->list) && count < 100) {
		entry = list_entry(head_ptr, struct event_struct, list);
		char *event_line = event_to_str(*entry);
		if (event_line) {
			ft_write_tmpfile(event_line, strlen(event_line));
			kfree(event_line);
		}
		head_ptr = head_ptr->next;
		count++;
	}
	spin_unlock(&devfile_io_spinlock);

	snprintf(stats_str, 512,
		"=== END LOG (Showing last %d of %d events) ===\n",
		count, total_events);
	ft_write_tmpfile(stats_str, strlen(stats_str));

	kfree(output_str);
	kfree(stats_str);
	
	if (tmpfile) {
		filp_close(tmpfile, NULL);
		tmpfile = NULL;
	}
}

void ft_log_tmpfile(void)
{
	char *temp_str;
	struct list_head *head_ptr;
	struct event_struct *entry;
	int total_inputs;

	head_ptr = g_driver->events_head->list.next;
	temp_str = kmalloc(69, GFP_KERNEL);
	total_inputs = 0;
	while (head_ptr != &(g_driver->events_head->list))
	{
		entry = list_entry(head_ptr, struct event_struct, list);

		if (entry->is_pressed)
		{
			if (entry->ascii_value > 0)
			{
				++total_inputs;
				ft_write_tmpfile(&(entry->ascii_value), 1);
			}
		}

		head_ptr = head_ptr->next;
	}
	sprintf(temp_str, "\n\nTotal keystrokes: %d\n", total_inputs);
	ft_write_tmpfile(temp_str, strlen(temp_str));
	kfree(temp_str);
}