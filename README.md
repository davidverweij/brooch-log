# brooch-log
physical connection logger for low-power Arduino projects

## Setup
A 4 layered [tinyduino](https://tinycircuits.com/pages/tinyduino-overview) stack, including (no particular order):

- tinyduino processor board
- tinyshield proto board
- real-time clock tinyshield
- miscrosd tinyshield

A 150 mAh battery is connected to the stack.

## How it works
The tinyshield protoboard has two 'floating' wires on the `V++` and `pin 4`, which - with an internal pullup, acts as a button (by dis-connecting them). The original code (see [old approach](/old-interrupt-approach)) would wake up the processor once the wires were (dis)connected - though this was draining a lot of power. Instead, the current approach wakes up every X minutes to check if the wires are (dis)connected (i.e. polling). If its state differs from before, it registers a (dis)connect. The timing (the 'X') can be adjusted according to your accuracy needs.

Using the 150 mAh, this tiny logger should survive for at least a month (based on measurements - not fully tested).

## Note
Upload the [RTC set time](/rtc-set-time) sketch to set the time on the RTC. **Without disconnecting**, upload the [brooch-log.ino](brooch-log.ino) to start logging!
