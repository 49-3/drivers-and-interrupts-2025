/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   42kb.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:19:44 by daribeir          #+#    #+#             */
/*   Updated: 2025/11/16 00:27:08 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/

#ifndef __42KB__H__
#define __42KB__H__

#include <linux/list.h> // list_head
#include <linux/workqueue.h> // work_struct
#include <linux/slab.h> // kmalloc and kfree
#include <linux/ktime.h>
#include <linux/timekeeping.h>

/**
 * ===== DRIVER CONFIGURATION =====
 */
#define KB_IRQ 1
#define KB_PORT 0x60

#define DEV_NAME "ft_module_keyboard"
#define DRV_NAME "ft_module_keyboard_driver"

#define MAX_SCANCODE_SIZE 0x80
#define READ_BUFFER_MAX 8192

/**
 * ===== MODULE INFO =====
 */
#define DRIVER_AUTHOR "david ribeiro daribeir@student.42mulhouse.fr"
#define DRIVER_DESC "Linux Keyboard Driver with HID support and advanced logging"
#define DRIVER_VERSION "2.0.0"
#define DRIVER_LICENSE "GPL"

/**
 * ===== END CONFIGURATION =====
 */

/**
 * ft_key - a representation of a key
 *
 * name - name of the key
 * caps_name - capital variation of the key name
 * ascii - ascii code of key, -1 if none
 * caps_ascii - ascii code of capital key, -1 if none
*/
typedef struct ft_key {
	char *name;
	char *caps_name;
	char ascii;
	char caps_ascii;
} ft_key;


/**
 * event_struct - a representation of a keystroke
 *
 * scan_code - the scan code produced by the keyboards irq pin.
 * is_pressed - flag wether this key is pressed or released
 * name - name of the key
 * time - timestamp of interrupt event
 * ascii_value - ascii representation of the key
 * list - kernel linked list pointer
*/
typedef struct event_struct {
	int scan_code;
	int is_pressed;
	char *name;
	time64_t time;
	char ascii_value; // char?
	struct list_head list;
} event_struct;

/**
 * drv_struct - the main struct of the driver, contains general information
 * about driver state
 *
 * events_head - head of the keylog event list
 * total_events - number of total events (bonus)
 * pid - pid
*/
typedef struct drv_struct {
	event_struct *events_head;
	int total_events;
	struct task_struct *task;
} drv_struct ;

/**
 * queue_data - struct that we will be passing into the workqueue function
 *
 * is_shift - flag is true when shift key is held down
 * is_caps - flag is true when caps lock is held down
 * driver - main driver struct
 * worker - container_of for workqueue function to extract data
*/
typedef struct queue_data
{
	int *is_shift;
	int *is_caps;
	drv_struct driver;
	struct work_struct worker;
} queue_data;

// Global refrences (module_init and module_exit does not share stack)
extern drv_struct *g_driver;
extern struct ft_key scancode_table[MAX_SCANCODE_SIZE];

// USB config (hot-plugging)
int ft_register_usb(void);
void ft_deregister_usb(void);

// Utilities
void ft_log(char *str);
void ft_warn(char *str);
drv_struct *ft_create_driver(void);
void ft_free_driver(drv_struct *driver);
drv_struct *ft_create_driver(void);
event_struct *ft_create_event(
	int scancode,
	int is_pressed,
	char *name,
	int time,
	int ascii_value
	);
queue_data *ft_create_q_data(int is_shift, int is_caps);
void ft_free_q_data(queue_data * q_data);
event_struct *ft_generate_event(queue_data q_data, int scancode);
char *event_to_str(event_struct event);
char	*ft_itoa(long long nbr);

// tmpfile functions
void ft_log_tmpfile_with_stats(void);
void ft_log_stats_to_kernel(void);

// Misc device handling
int ft_create_misc_device(void);
void ft_destroy_misc_device(void);

// interrupt handling
int ft_register_interrupt(void);
void ft_deregister_interrupt(void);

// tmpfile handling
int ft_create_tmpfile(void);
void ft_destroy_tmpfile(void);
void ft_log_tmpfile(void);

// test

#endif  //!__42KB__H__