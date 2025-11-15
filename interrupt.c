/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   interrupt.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:20:05 by daribeir          #+#    #+#             */
/*   Updated: 2025/11/15 23:22:20 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/


#include <linux/interrupt.h>
#include <linux/irqnr.h>
#include <asm/io.h>
#include <linux/input.h>
#include <linux/spinlock_types.h>
#include "42kb.h"

extern spinlock_t devfile_io_spinlock;
DEFINE_SPINLOCK(q_data_spinlock);
queue_data *q_data;
int is_caps = 0;
int is_shift = 0;

/**
 * read_key - Actual logic for handling keypresses
*/
void read_key(struct work_struct *workqueue)
{
	int scancode = inb(KB_PORT);
	event_struct *event;

	queue_data *q_data = container_of(workqueue, queue_data, worker);

	// preprocessing
	spin_lock(&q_data_spinlock);
	q_data->is_caps = &is_caps;
	q_data->is_shift = &is_shift;
	spin_unlock(&q_data_spinlock);

	// event creation
	event = ft_generate_event(*q_data, scancode);

	// event storing - AVEC PROTECTION
	spin_lock(&devfile_io_spinlock);
	list_add_tail(&(event->list), &(q_data->driver.events_head->list));
	g_driver->total_events++;
	spin_unlock(&devfile_io_spinlock);

	// post processing (shift, caps)
	spin_lock(&q_data_spinlock);

	// if event name is caps, toggle caps
	if (!strcmp("caps", event->name) && !event->is_pressed)
		is_caps = !is_caps;
	if (!strcmp("shift", event->name))
	{
		if (event->is_pressed)
			is_shift = 1;
		else
			is_shift = 0;
	}
	spin_unlock(&q_data_spinlock);
}

irqreturn_t handler(int irq, void *dev_id){
	// call workqueue here
	schedule_work(&(q_data->worker));

	return IRQ_HANDLED;
}

int ft_register_interrupt(void)
{
	// declare queue data
	q_data = ft_create_q_data(0, 0);
	if (!q_data)
	{
		ft_warn("Queue data creation failed\n");
		return 1;
	}
	q_data->driver = *(g_driver);
	// declare work queue action
	INIT_WORK(&(q_data->worker), read_key);
	return request_irq(KB_IRQ, &handler, IRQF_SHARED, "ft_kb", g_driver);
}

void ft_deregister_interrupt(void)
{
	ft_free_q_data(q_data);
	free_irq(KB_IRQ, g_driver);
}