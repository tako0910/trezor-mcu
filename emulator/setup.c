/*
 * This file is part of the TREZOR project, https://trezor.io/
 *
 * Copyright (C) 2017 Saleem Rashid <trezor@saleemrashid.com>
 * Modified Copyright (C) 2018 Yannick Heneault <yheneaul@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <libopencm3/stm32/flash.h>

#include "memory.h"
#include "oled.h"
#include "rng.h"
#include "setup.h"
#include "timer.h"

#define EMULATOR_FLASH_FILE "emulator.img"

#ifndef RANDOM_DEV_FILE
#define RANDOM_DEV_FILE "/dev/urandom"
#endif

uint8_t *emulator_flash_base = NULL;

uint32_t __stack_chk_guard;

static int random_fd = -1;

static void setup_urandom(void);
static void setup_flash(void);

#ifdef PIZERO

#include <bcm2835.h>
#include "buttons.h"

static uint8_t gpio_yes;
static uint8_t gpio_no;

static uint8_t buttonPin(const char* pinVarName, uint8_t defaultPin) {
	int pin = defaultPin;
	const char *variable = getenv(pinVarName);
	if (variable != NULL) {
		int gpio = atoi(variable);
		if (gpio >= 1 && pin <=27) {
			pin = gpio;
		} else {
			fprintf(stderr, "Invalid value in config file for %s. Must be between 1 and 27.\n", pinVarName);
			exit(1);
		}
	}

	return pin;
}

uint16_t buttonRead(void) {
	uint16_t state = 0;

	state |= bcm2835_gpio_lev(gpio_no) == 0 ? BTN_PIN_NO : 0;
	state |= bcm2835_gpio_lev(gpio_yes) == 0 ? BTN_PIN_YES : 0;
	return ~state;
}

#endif


void setup(void) {
	setup_urandom();
	setup_flash();
#ifdef PIZERO
	bcm2835_init();
	gpio_yes = buttonPin("TREZOR_GPIO_YES", 16);
	bcm2835_gpio_fsel(gpio_yes, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_set_pud(gpio_yes, BCM2835_GPIO_PUD_UP );
	gpio_no = buttonPin("TREZOR_GPIO_NO", 12);
	bcm2835_gpio_fsel(gpio_no, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_set_pud(gpio_no, BCM2835_GPIO_PUD_UP );
#endif
}

void __attribute__((noreturn)) shutdown(void) {
	for(;;) pause();
}

void emulatorRandom(void *buffer, size_t size) {
	ssize_t n, len = 0;
	do {
		n = read(random_fd, (char*)buffer + len, size - len);
		if (n < 0) {
			perror("Failed to read " RANDOM_DEV_FILE);
			exit(1);
		}
		len += n;
	} while (len != (ssize_t)size);
}

static void setup_urandom(void) {
	random_fd = open(RANDOM_DEV_FILE, O_RDONLY);
	if (random_fd < 0) {
		perror("Failed to open " RANDOM_DEV_FILE);
		exit(1);
	}
}

static void setup_flash(void) {
	int fd = open(EMULATOR_FLASH_FILE, O_RDWR | O_SYNC | O_CREAT, 0644);
	if (fd < 0) {
		perror("Failed to open flash emulation file");
		exit(1);
	}

	off_t length = lseek(fd, 0, SEEK_END);
	if (length < 0) {
		perror("Failed to read length of flash emulation file");
		exit(1);
	}

	emulator_flash_base = mmap(NULL, FLASH_TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (emulator_flash_base == MAP_FAILED) {
		perror("Failed to map flash emulation file");
		exit(1);
	}

	if (length < FLASH_TOTAL_SIZE) {
		if (ftruncate(fd, FLASH_TOTAL_SIZE) != 0) {
			perror("Failed to initialize flash emulation file");
			exit(1);
		}

		/* Initialize the flash */
		flash_erase_all_sectors(FLASH_CR_PROGRAM_X32);
	}
}
