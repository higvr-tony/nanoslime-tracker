#include "globals.h"

#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "haptics.h"

LOG_MODULE_REGISTER(haptics, LOG_LEVEL_INF);

#if DT_HAS_COMPAT_STATUS_OKAY(nanoslime_haptic_motor)
#define HAPTICS_EXISTS true
#define HAPTICS_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(nanoslime_haptic_motor)
static const struct pwm_dt_spec haptic_pwm = {
	.dev = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(HAPTICS_NODE, haptic_pwms, 0)),
	.channel = DT_PHA_BY_IDX(HAPTICS_NODE, haptic_pwms, 0, channel),
	.period = DT_PHA_BY_IDX(HAPTICS_NODE, haptic_pwms, 0, period),
	.flags = DT_PHA_BY_IDX(HAPTICS_NODE, haptic_pwms, 0, flags),
};
#endif

//|b0      |b1      |b2      |b3      |b4      |...     |b30     |b31     |
//|ACK 0xA7|seq     |id 0    |int 0   |id 1    |...     |id 14   |int 14  |

#define HAPTICS_MAX_ENTRIES 15
#define HAPTICS_MIN_DUTY_PPT 350 // coin ERM motors stall below ~35% duty
#define HAPTICS_FAILSAFE_TIMEOUT K_MSEC(250) // stop if no table was received

#ifdef HAPTICS_EXISTS
static atomic_t haptics_intensity = ATOMIC_INIT(0);
static uint8_t haptics_applied = 0;

static void haptics_apply(struct k_work *work);
static K_WORK_DEFINE(haptics_apply_work, haptics_apply);
static void haptics_failsafe(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(haptics_failsafe_work, haptics_failsafe);

static void haptics_apply(struct k_work *work)
{
	uint8_t intensity = atomic_get(&haptics_intensity);
	if (intensity == haptics_applied)
		return;
	if (!device_is_ready(haptic_pwm.dev))
	{
		LOG_WRN_ONCE("Haptic PWM device not ready");
		return;
	}
	uint32_t pulse = 0;
	if (intensity) // map 1-255 onto min duty to 100%
		pulse = (uint64_t)haptic_pwm.period * (HAPTICS_MIN_DUTY_PPT + (1000 - HAPTICS_MIN_DUTY_PPT) * (intensity - 1) / 254) / 1000;
	int err = pwm_set_pulse_dt(&haptic_pwm, pulse);
	if (err)
	{
		LOG_ERR("Failed to set haptic PWM: %d", err);
		return;
	}
	if (!haptics_applied || !intensity)
		LOG_DBG("Haptics %s", intensity ? "on" : "off");
	haptics_applied = intensity;
}

static void haptics_failsafe(struct k_work *work)
{
	atomic_set(&haptics_intensity, 0);
	haptics_apply(NULL);
}
#endif

void haptics_parse_table(const uint8_t *data, uint8_t len, uint8_t tracker_id)
{
#ifdef HAPTICS_EXISTS
	if (len < 2 || (len & 1))
		return;
	uint8_t count = (len - 2) / 2;
	if (count > HAPTICS_MAX_ENTRIES)
		count = HAPTICS_MAX_ENTRIES;
	uint8_t intensity = 0; // absent = stop
	for (int i = 0; i < count; i++)
	{
		if (data[2 + i * 2] == tracker_id)
		{
			intensity = data[3 + i * 2];
			break;
		}
	}
	k_work_reschedule(&haptics_failsafe_work, HAPTICS_FAILSAFE_TIMEOUT);
	if (atomic_set(&haptics_intensity, intensity) != intensity)
		k_work_submit(&haptics_apply_work);
#else
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	ARG_UNUSED(tracker_id);
#endif
}
