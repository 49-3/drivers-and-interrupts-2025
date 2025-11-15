#******************************************************************************#
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/28 01:20:24 by daribeir          #+#    #+#              #
#    Updated: 2025/04/28 01:20:25 by daribeir         ###   ##### Mulhouse.fr  #
#                                                                              #
#******************************************************************************#

obj-m += 42kb.o
42kb-y := main.o usb.o utils.o device.o interrupt.o tmpfile.o

KDIR = /lib/modules/$(shell uname -r)/build

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
