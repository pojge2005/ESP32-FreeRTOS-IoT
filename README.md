# ESP32 FreeRTOS + MQTT IoT Demonstration

A learning project demonstrating task-based firmware on an ESP32: simulated sensor readings, FreeRTOS queues, a push-button ISR with a binary semaphore, and MQTT messaging.

> **Status:** Educational prototype. The current code change has not yet been compiled or validated on hardware. Do not treat it as production-ready.

## Current features

- **Sensor task:** generates a simulated integer in the range `20–39` about every three seconds and sends it without waiting to two bounded queues: one for LED logic and one for MQTT publishing.
- **LED task:** consumes its own queue and turns the LED on when a value is greater than `30`. A push-button interrupt gives a binary semaphore; a timed queue wait lets the LED task service button events even when no sensor values arrive.
- **MQTT task:** is the sole owner of PubSubClient calls, connects to Wi-Fi and the broker, subscribes to `esp32/led`, handles `ON` / `OFF` commands, and drains the outbound sensor queue when connected.
- **Sensor topic:** publishes queued simulated values to `esp32/sensor` when MQTT is connected. Failed publish calls are counted and are not retried indefinitely.

## Hardware and software

- ESP32 development board supported by the selected Arduino-ESP32 core
- LED on GPIO 2 (confirm the board's LED wiring before use)
- Momentary button from GPIO 4 to GND; the sketch uses `INPUT_PULLUP`
- Arduino IDE or PlatformIO
- `PubSubClient` library

Pin assignments are defined at the top of `FreeRTOS-ESP32Project.ino`. Check for pin conflicts on your specific ESP32 board before wiring.

## Build and run

1. Install the ESP32 board package for Arduino and the `PubSubClient` library.
2. Open `FreeRTOS-ESP32Project.ino`.
3. Replace `YOUR_WIFI` and `YOUR_PASS` with local Wi-Fi credentials. Do not commit real credentials.
4. Review `mqtt_server` and choose a broker you are authorized to use.
5. Connect the LED and button using the pin assignments above.
6. Select the correct ESP32 board and serial port, upload the sketch, and open Serial Monitor at **115200 baud**.
7. With an MQTT client, subscribe to `esp32/sensor` and publish `ON` or `OFF` to `esp32/led`.

The sketch uses `broker.hivemq.com` on port `1883` as an example public broker. Public brokers are shared and do not provide confidentiality or access control guarantees for this demo. Use a private broker with authentication and TLS for real deployments. Use a unique topic prefix to avoid collisions.

## Message contract

| Direction | Topic | Payload | Behavior |
| --- | --- | --- | --- |
| Device → broker | `esp32/sensor` | Decimal integer, `20–39` | Simulated value about every 3 seconds when connected |
| Broker → device | `esp32/led` | `ON` or `OFF` | MQTT callback sets GPIO 2 high or low |

Payload matching is case-sensitive.

## Architecture

```text
                   +--> bounded LED queue --> LED Task --> GPIO 2
Sensor Task -------|
                   +--> bounded MQTT queue --> MQTTTask --> PubSubClient
                                                              |
Button GPIO 4 --> ISR --> binary semaphore --> LED Task       +--> esp32/sensor
                                                              +<-- esp32/led
```

## Queue and reconnect policy

- Both queues have depth **5** and store integer readings.
- Queue sends use zero wait. If a queue is full, the newest reading is dropped and its corresponding drop counter is incremented. Sampling does not wait for queue space or MQTT recovery.
- Only `MQTTTask` calls PubSubClient methods, including `connect()`, `subscribe()`, `connected()`, `loop()`, `publish()`, and `state()`.
- MQTT reconnect attempts use exponential backoff from 1 second up to 30 seconds. The PubSubClient socket timeout is configured to 3 seconds.
- Wi-Fi reconnection relies on Arduino-ESP32's auto-reconnect behavior; validate it on the selected core and board.
- Failed publishes increment a counter and are not retried indefinitely. Delivery is **best-effort**, not guaranteed.
- Drop/failure counters are volatile runtime diagnostics and reset at boot; they are not persisted or currently exposed over MQTT.
- The LED can still be written by both the LED task and MQTT callback. A single LED-state owner remains future work.

## Known limitations

1. **Compile and hardware verification are pending.** Record the Arduino-ESP32 core and PubSubClient versions and test on the actual target board before claiming success.
2. **Best-effort delivery only.** Readings can be dropped when a queue is full, while disconnected, or after a publish failure.
3. **LED has multiple writers.** The LED task and MQTT callback can both change GPIO 2. Define one LED-state owner and command/sensor priority before extending the demo.
4. **Button debounce is not implemented.** Test switch bounce and consider a timer or task-level debounce.
5. **Wi-Fi recovery depends on core behavior.** The sketch relies on Wi-Fi auto-reconnect and does not expose a formal Wi-Fi state machine.
6. **Data is simulated.** Replace `random(20, 40)` with a real sensor read only after documenting the sensor, wiring, units, and error handling.
7. **Public broker is demo-only.** Use authenticated TLS-protected infrastructure for real deployments.

## Verification checklist

- [ ] Sketch compiles with recorded Arduino-ESP32 core and PubSubClient versions.
- [ ] Board and GPIO assignments have been checked against actual hardware.
- [ ] Wi-Fi and MQTT connection behavior has been observed.
- [ ] MQTT command handling has been tested with `ON` and `OFF`.
- [ ] Queue overflow, broker disconnect/reconnect, and backoff have been tested.
- [ ] Button bounce and queue-full behavior have been tested.
- [ ] No real Wi-Fi credentials or broker secrets are committed.

## Contribution workflow

Use a focused branch for each change. Include board/core/library versions, reproduction steps, expected behavior, observed result, and test evidence in pull requests. Do not describe hardware tests as passed unless they were actually run.

---

**License:** No license is specified in this repository yet. Ask the maintainer to choose a license before redistributing or reusing this project.
