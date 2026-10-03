// SPDX-License-Identifier: BSD-2-Clause
/*
 * optee-i2c-test: exercise the OP-TEE I2C device drivers from Linux
 * through the i2c_test TA and the i2c_devices PTA.
 */
#include <errno.h>
#include <getopt.h>
#include <i2c_test_ta.h>
#include <pta_i2c_devices.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tee_client_api.h>
#include <time.h>

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#define VAL_IN		TEEC_VALUE_INPUT
#define VAL_OUT		TEEC_VALUE_OUTPUT
#define MEM_IN		TEEC_MEMREF_TEMP_INPUT
#define MEM_OUT		TEEC_MEMREF_TEMP_OUTPUT
#define NONE		TEEC_NONE

static TEEC_Context ctx;
static TEEC_Session sess;
static uint32_t dev_idx;

static const char *const class_names[] = {
	[PTA_I2C_DEVICES_CLASS_TEMP] = "temperature (lm75)",
	[PTA_I2C_DEVICES_CLASS_GPIO] = "gpio (mcp23008)",
	[PTA_I2C_DEVICES_CLASS_RTC] = "rtc (mcp7940x)",
	[PTA_I2C_DEVICES_CLASS_EEPROM] = "eeprom (at24)",
	[PTA_I2C_DEVICES_CLASS_ADC] = "adc/dac (pcf8591)",
};

static const char *res_str(TEEC_Result res)
{
	switch (res) {
	case TEEC_SUCCESS:
		return "ok";
	case TEEC_ERROR_ITEM_NOT_FOUND:
		return "no such device";
	case TEEC_ERROR_BAD_STATE:
		return "device NACK / not ready";
	case TEEC_ERROR_BUSY:
		return "bus timeout";
	case TEEC_ERROR_BAD_PARAMETERS:
		return "bad parameters";
	case TEEC_ERROR_NOT_SUPPORTED:
		return "not supported (driver disabled?)";
	case TEEC_ERROR_ACCESS_DENIED:
		return "access denied";
	default:
		return "error";
	}
}

static TEEC_Result invoke(uint32_t cmd, TEEC_Operation *op)
{
	uint32_t origin = 0;
	TEEC_Result res = TEEC_InvokeCommand(&sess, cmd, op, &origin);

	if (res)
		fprintf(stderr, "command %u failed: 0x%08x (%s), origin %u\n",
			cmd, res, res_str(res), origin);

	return res;
}

static TEEC_Result invoke_vv(uint32_t cmd, uint32_t t1, uint32_t a, uint32_t b,
			     uint32_t a1, uint32_t b1, uint32_t *out)
{
	TEEC_Operation op = { };
	TEEC_Result res = TEEC_ERROR_GENERIC;

	op.paramTypes = TEEC_PARAM_TYPES(VAL_IN, t1, NONE, NONE);
	op.params[0].value.a = a;
	op.params[0].value.b = b;
	op.params[1].value.a = a1;
	op.params[1].value.b = b1;

	res = invoke(cmd, &op);
	if (!res && out)
		*out = op.params[1].value.a;

	return res;
}

static TEEC_Result invoke_mem(uint32_t cmd, uint32_t t1, uint32_t a,
			      uint32_t b, void *buf, size_t *len)
{
	TEEC_Operation op = { };
	TEEC_Result res = TEEC_ERROR_GENERIC;

	op.paramTypes = TEEC_PARAM_TYPES(VAL_IN, t1, NONE, NONE);
	op.params[0].value.a = a;
	op.params[0].value.b = b;
	op.params[1].tmpref.buffer = buf;
	op.params[1].tmpref.size = *len;

	res = invoke(cmd, &op);
	*len = op.params[1].tmpref.size;

	return res;
}

static unsigned long parse_ul(const char *s)
{
	char *end = NULL;
	unsigned long v = 0;

	errno = 0;
	v = strtoul(s, &end, 0);
	if (errno || !*s || *end) {
		fprintf(stderr, "invalid number: %s\n", s);
		exit(EXIT_FAILURE);
	}

	return v;
}

static void hexdump(size_t base, const uint8_t *buf, size_t len)
{
	size_t n = 0;

	for (n = 0; n < len; n++) {
		if (!(n % 16))
			printf("%s%04zx:", n ? "\n" : "", base + n);
		printf(" %02x", buf[n]);
	}
	printf("\n");
}

static int cmd_list(void)
{
	uint32_t cls = 0;
	uint32_t count = 0;

	for (cls = 0; cls < sizeof(class_names) / sizeof(class_names[0]);
	     cls++) {
		if (invoke_vv(PTA_I2C_DEVICES_CMD_COUNT, VAL_OUT, cls, 0, 0, 0,
			      &count))
			return 1;
		printf("%-20s %u\n", class_names[cls], count);
	}

	return 0;
}

static int cmd_temp(void)
{
	uint32_t mc = 0;

	if (invoke_vv(PTA_I2C_DEVICES_CMD_TEMP_GET, VAL_OUT, dev_idx, 0, 0, 0,
		      &mc))
		return 1;

	printf("temperature: %.3f C\n", (int32_t)mc / 1000.0);

	return 0;
}

static int cmd_rtc_get(void)
{
	static const char *const wdays[] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
	};
	struct pta_i2c_devices_time tm = { };
	size_t len = sizeof(tm);

	if (invoke_mem(PTA_I2C_DEVICES_CMD_RTC_GET, MEM_OUT, dev_idx, 0, &tm,
		       &len))
		return 1;

	printf("rtc: %s %04u-%02u-%02u %02u:%02u:%02u UTC\n",
	       tm.wday < 7 ? wdays[tm.wday] : "???", tm.year, tm.mon + 1,
	       tm.mday, tm.hour, tm.min, tm.sec);

	return 0;
}

static int cmd_rtc_set(void)
{
	struct pta_i2c_devices_time tm = { };
	size_t len = sizeof(tm);
	time_t now = time(NULL);
	struct tm utc = { };

	gmtime_r(&now, &utc);
	tm = (struct pta_i2c_devices_time){
		.year = utc.tm_year + 1900, .mon = utc.tm_mon,
		.mday = utc.tm_mday, .wday = utc.tm_wday,
		.hour = utc.tm_hour, .min = utc.tm_min, .sec = utc.tm_sec,
	};

	if (invoke_mem(PTA_I2C_DEVICES_CMD_RTC_SET, MEM_IN, dev_idx, 0, &tm,
		       &len))
		return 1;

	printf("rtc set from system time\n");

	return cmd_rtc_get();
}

static int cmd_eeprom_size(void)
{
	uint32_t size = 0;

	if (invoke_vv(PTA_I2C_DEVICES_CMD_EEPROM_SIZE, VAL_OUT, dev_idx, 0, 0,
		      0, &size))
		return 1;

	printf("eeprom size: %u bytes\n", size);

	return 0;
}

static int cmd_eeprom_read(unsigned long off, unsigned long len)
{
	uint8_t *buf = NULL;
	size_t sz = len;
	int ret = 1;

	if (!len || len > TA_I2C_TEST_MAX_BUF) {
		fprintf(stderr, "length must be 1..%d\n", TA_I2C_TEST_MAX_BUF);
		return 1;
	}

	buf = calloc(1, len);
	if (!buf)
		return 1;

	if (!invoke_mem(PTA_I2C_DEVICES_CMD_EEPROM_READ, MEM_OUT, dev_idx, off,
			buf, &sz)) {
		hexdump(off, buf, len);
		ret = 0;
	}

	free(buf);

	return ret;
}

static int cmd_eeprom_write(unsigned long off, const char *hex)
{
	uint8_t buf[TA_I2C_TEST_MAX_BUF] = { };
	size_t len = strlen(hex) / 2;
	unsigned int byte = 0;
	size_t n = 0;

	if (!len || strlen(hex) % 2 || len > sizeof(buf)) {
		fprintf(stderr, "data must be an even number of hex digits\n");
		return 1;
	}

	for (n = 0; n < len; n++) {
		if (sscanf(hex + 2 * n, "%2x", &byte) != 1) {
			fprintf(stderr, "invalid hex data\n");
			return 1;
		}
		buf[n] = byte;
	}

	if (invoke_mem(PTA_I2C_DEVICES_CMD_EEPROM_WRITE, MEM_IN, dev_idx, off,
		       buf, &len))
		return 1;

	printf("wrote %zu bytes at 0x%lx\n", strlen(hex) / 2, off);

	return cmd_eeprom_read(off, strlen(hex) / 2);
}

static int cmd_gpio_config(unsigned long pin, const char *dir, bool pullup)
{
	uint32_t flags = pullup ? PTA_I2C_DEVICES_GPIO_PULLUP : 0;

	if (!strcmp(dir, "out"))
		flags |= PTA_I2C_DEVICES_GPIO_OUTPUT;
	else if (strcmp(dir, "in"))
		return fprintf(stderr, "direction must be in or out\n"), 1;

	return !!invoke_vv(PTA_I2C_DEVICES_CMD_GPIO_CONFIG, VAL_IN, dev_idx,
			   pin, flags, 0, NULL);
}

static int cmd_gpio_get(unsigned long pin)
{
	uint32_t level = 0;

	if (invoke_vv(PTA_I2C_DEVICES_CMD_GPIO_GET, VAL_OUT, dev_idx, pin, 0,
		      0, &level))
		return 1;

	printf("gpio %lu: %u\n", pin, level);

	return 0;
}

static int cmd_adc(unsigned long ch)
{
	uint32_t val = 0;

	if (invoke_vv(PTA_I2C_DEVICES_CMD_ADC_READ, VAL_OUT, dev_idx, ch, 0, 0,
		      &val))
		return 1;

	printf("adc ch%lu: %u (%.1f%%)\n", ch, val, val * 100.0 / 255);

	return 0;
}

static int cmd_dac(const char *arg)
{
	bool off = !strcmp(arg, "off");

	return !!invoke_vv(PTA_I2C_DEVICES_CMD_DAC_WRITE, VAL_IN, dev_idx, 0,
			   off ? 0 : parse_ul(arg), !off, NULL);
}

/* Read-only checks of every device class */
static int cmd_selftest(void)
{
	unsigned long n = 0;
	int fails = 0;

	fails += cmd_list();
	fails += cmd_temp();
	fails += cmd_rtc_get();
	fails += cmd_eeprom_size();
	fails += cmd_eeprom_read(0, 16);
	for (n = 0; n < 4; n++)
		fails += cmd_adc(n);
	for (n = 0; n < 8; n++)
		fails += cmd_gpio_get(n);

	printf("selftest: %d failure(s)\n", fails);

	return !!fails;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s [-d INDEX] COMMAND [ARGS]\n"
		"  -d INDEX                device index in its class (default 0)\n"
		"commands:\n"
		"  list                    number of devices per class\n"
		"  selftest                read-only check of every device\n"
		"  temp                    read temperature\n"
		"  rtc-get                 read RTC time\n"
		"  rtc-set                 set RTC from system time (UTC)\n"
		"  eeprom-size             EEPROM size\n"
		"  eeprom-read OFF LEN     hexdump LEN bytes at OFF\n"
		"  eeprom-write OFF HEX    write hex bytes at OFF\n"
		"  gpio-config PIN in|out [pullup]\n"
		"  gpio-set PIN 0|1\n"
		"  gpio-get PIN\n"
		"  adc CH                  read ADC channel 0..3\n"
		"  dac VALUE|off           drive DAC output 0..255\n",
		prog);
	exit(EXIT_FAILURE);
}

static int do_list(char **a __unused)
{
	return cmd_list();
}

static int do_selftest(char **a __unused)
{
	return cmd_selftest();
}

static int do_temp(char **a __unused)
{
	return cmd_temp();
}

static int do_rtc_get(char **a __unused)
{
	return cmd_rtc_get();
}

static int do_rtc_set(char **a __unused)
{
	return cmd_rtc_set();
}

static int do_eeprom_size(char **a __unused)
{
	return cmd_eeprom_size();
}

static int do_eeprom_read(char **a)
{
	return cmd_eeprom_read(parse_ul(a[0]), parse_ul(a[1]));
}

static int do_eeprom_write(char **a)
{
	return cmd_eeprom_write(parse_ul(a[0]), a[1]);
}

static int do_gpio_config(char **a)
{
	return cmd_gpio_config(parse_ul(a[0]), a[1], false);
}

static int do_gpio_config_pullup(char **a)
{
	if (strcmp(a[2], "pullup"))
		return fprintf(stderr, "expected pullup\n"), 1;

	return cmd_gpio_config(parse_ul(a[0]), a[1], true);
}

static int do_gpio_set(char **a)
{
	return !!invoke_vv(PTA_I2C_DEVICES_CMD_GPIO_SET, VAL_IN, dev_idx,
			   parse_ul(a[0]), parse_ul(a[1]), 0, NULL);
}

static int do_gpio_get(char **a)
{
	return cmd_gpio_get(parse_ul(a[0]));
}

static int do_adc(char **a)
{
	return cmd_adc(parse_ul(a[0]));
}

static int do_dac(char **a)
{
	return cmd_dac(a[0]);
}

static const struct {
	const char *name;
	int nargs;
	int (*fn)(char **args);
} commands[] = {
	{ "list", 0, do_list },
	{ "selftest", 0, do_selftest },
	{ "temp", 0, do_temp },
	{ "rtc-get", 0, do_rtc_get },
	{ "rtc-set", 0, do_rtc_set },
	{ "eeprom-size", 0, do_eeprom_size },
	{ "eeprom-read", 2, do_eeprom_read },
	{ "eeprom-write", 2, do_eeprom_write },
	{ "gpio-config", 2, do_gpio_config },
	{ "gpio-config", 3, do_gpio_config_pullup },
	{ "gpio-set", 2, do_gpio_set },
	{ "gpio-get", 1, do_gpio_get },
	{ "adc", 1, do_adc },
	{ "dac", 1, do_dac },
};

static int run(int argc, char *argv[])
{
	size_t n = 0;

	for (n = 0; n < sizeof(commands) / sizeof(commands[0]); n++)
		if (!strcmp(argv[0], commands[n].name) &&
		    argc - 1 == commands[n].nargs)
			return commands[n].fn(argv + 1);

	usage("optee-i2c-test");
	return 1;
}

int main(int argc, char *argv[])
{
	const TEEC_UUID uuid = TA_I2C_TEST_UUID;
	TEEC_Result res = TEEC_ERROR_GENERIC;
	uint32_t origin = 0;
	int opt = 0;
	int ret = 0;

	while ((opt = getopt(argc, argv, "d:h")) != -1) {
		if (opt == 'd')
			dev_idx = parse_ul(optarg);
		else
			usage(argv[0]);
	}
	if (optind >= argc)
		usage(argv[0]);

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res) {
		fprintf(stderr, "TEEC_InitializeContext: 0x%08x\n", res);
		return EXIT_FAILURE;
	}

	res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL,
			       NULL, &origin);
	if (res) {
		fprintf(stderr, "TEEC_OpenSession: 0x%08x (%s), origin %u\n",
			res, res_str(res), origin);
		TEEC_FinalizeContext(&ctx);
		return EXIT_FAILURE;
	}

	ret = run(argc - optind, argv + optind);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return ret ? EXIT_FAILURE : EXIT_SUCCESS;
}
