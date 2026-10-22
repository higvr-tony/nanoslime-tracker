# Haptics (NanoSlime tracker fork)

Trackers with a vibration motor vibrate when the receiver's haptics state
table lists their tracker id. Protocol: `docs/haptics.md` in the NanoSlime
hardware repo.

## Receiving

`src/connection/esb.c` passes every ACK payload with length ≥ 2 and
byte 0 = `0xA7` to `haptics_parse_table()` (`src/system/haptics.c`):

```
b0 0xA7 | b1 seq | b2 id0 | b3 int0 | ... up to 15 entries
```

The tracker looks up its own id (from pairing); absent = intensity 0.
The RX handler now drains the RX FIFO instead of reading one payload.

## Motor driver

- Devicetree: a node with `compatible = "nanoslime,haptic-motor"` and a
  `haptic-pwms` property (binding in `dts/bindings/`). Without such a node
  the driver compiles out to an empty `haptics_parse_table()`.
- Duty: intensity 1–255 maps linearly onto 35–100 % (coin ERM motors stall
  below ~35 %), 0 = off.
- Failsafe: the motor stops if no table arrives for 250 ms.
- PWM is updated from a work item, the parser is ISR-safe.

Use a separate PWM instance at ~20 kHz: nRF PWM channels in one instance
share a period.

```dts
haptics {
	compatible = "nanoslime,haptic-motor";
	haptic-pwms = <&pwm2 0 PWM_USEC(50) PWM_POLARITY_NORMAL>;
};
```

(The NanoSlime v0.4 board, `nanoslime_uf2` in the hardware repo, uses P1.09
on PWM2 channel 0; PWM0 drives the LEDs, PWM1 the IMU CLKIN.)

## Building

Paths with spaces break devicetree preprocessing; build from a symlink:

```
west build -p -b nanoslime_uf2 -s ~/ncs/src/nanoslime-tracker -- -DBOARD_ROOT=<hardware repo>/firmware
```
