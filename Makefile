#******************************************************************************#
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/28 01:20:24 by daribeir          #+#    #+#              #
#    Updated: 2025/11/16 00:46:23 by daribeir         ###   ##### Mulhouse.fr  #
#                                                                              #
#******************************************************************************#

.SILENT:

obj-m += 42kb.o
42kb-y := main.o usb.o utils.o device.o interrupt.o tmpfile.o

KDIR = /lib/modules/$(shell uname -r)/build
UDEV_RULES_DIR = /etc/udev/rules.d
RULES_FILE = 79-usb.rules

all:
	@echo "🔨 Compiling module..."
	@make --no-print-directory -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules > /dev/null 2>&1
	@if [ -f "42kb.ko" ]; then \
		echo "✅ Compilation successful (42kb.ko generated)"; \
	else \
		echo "❌ Compilation failed"; \
		exit 1; \
	fi
	@echo ""

clean:
	@echo "🧹 Cleaning build files..."
	@make --no-print-directory -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean > /dev/null 2>&1
	@echo "✅ Clean complete"
	@echo ""

install: all install-udev
	@echo "📦 Installing module..."
	@install -D -m 644 42kb.ko /lib/modules/$(shell uname -r)/kernel/drivers/misc/42kb.ko 2>/dev/null
	@depmod -a > /dev/null 2>&1
	@$(MAKE) test-install --silent

install-udev:
	@echo "📝 Installing udev rules..."
	@if [ ! -d "$(UDEV_RULES_DIR)" ]; then \
		mkdir -p $(UDEV_RULES_DIR); \
	fi
	@install -D -m 644 $(RULES_FILE) $(UDEV_RULES_DIR)/$(RULES_FILE) 2>/dev/null
	@udevadm control --reload-rules > /dev/null 2>&1
	@udevadm trigger > /dev/null 2>&1

test-install:
	@echo ""
	@if [ -f "/lib/modules/$(shell uname -r)/kernel/drivers/misc/42kb.ko" ]; then \
		echo "  ✅ Module installed"; \
	else \
		echo "  ❌ Module installation FAILED"; \
		exit 1; \
	fi
	@if [ -f "$(UDEV_RULES_DIR)/$(RULES_FILE)" ]; then \
		echo "  ✅ Udev rules installed"; \
	else \
		echo "  ❌ Udev rules installation FAILED"; \
		exit 1; \
	fi
	@echo ""
	@echo "🎉 Installation SUCCESSFUL"
	@echo ""

uninstall:
	@echo ""
	@echo "🗑️  Uninstalling module..."
	@$(MAKE) test-uninstall --silent
	@rm -f /lib/modules/$(shell uname -r)/kernel/drivers/misc/42kb.ko 2>/dev/null
	@rm -f $(UDEV_RULES_DIR)/$(RULES_FILE) 2>/dev/null
	@depmod -a > /dev/null 2>&1
	@udevadm control --reload-rules > /dev/null 2>&1
	@udevadm trigger > /dev/null 2>&1
	@echo ""
	@echo "✅ Module uninstalled"
	@echo "✅ Udev rules removed"
	@echo ""
	@echo "🎉 Uninstallation SUCCESSFUL"
	@echo ""

test-uninstall:
	@if [ ! -f "/lib/modules/$(shell uname -r)/kernel/drivers/misc/42kb.ko" ]; then \
		echo "  ✅ Module not installed"; \
	fi
	@if [ ! -f "$(UDEV_RULES_DIR)/$(RULES_FILE)" ]; then \
		echo "  ✅ Udev rules not installed"; \
	fi

.PHONY: all clean install install-udev uninstall test-install test-uninstall
