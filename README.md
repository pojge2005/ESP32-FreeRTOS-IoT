# ESP32 FreeRTOS + MQTT IoT Demonstration

A learning project demonstrating task-based firmware on an ESP32: a simulated sensor producer, a queue, a push-button ISR with a binary semaphore, and MQTT messaging.

> **Status:** Educational prototype. The documentation describes the current sketch; it does not claim hardware validation or production readiness.

## Current features

- **Sensor task:** generates a simulated integer in the range `20–39` every three seconds and places it in a FreeRTOS queue.
- **LED task:** consumes queued values and turns the LED on when a value is greater than `30`. A push-button interrupt gives a binary semaphore; the LED task consumes it and toggles the LED.
- **MQTT task:** connects to Wi-Fi and an MQTT broker, subscribes to `esp32/led`, and processes `ON` / `OFF` commands.
- **Sensor topic:** publishes the simulated value to `esp32/sensor`.

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

The current sketch uses `broker.hivemq.com` on port `1883` as an example public broker. Public brokers are shared and generally do not provide confidentiality or access control for this demo. Use a private broker with authentication and TLS for real deployments. Use a unique topic prefix to avoid collisions with other users.

## Message contract

| Direction | Topic | Payload | Current behavior |
| --- | --- | --- | --- |
| Device → broker | `esp32/sensor` | Decimal integer, `20–39` | Simulated sensor value every ~3 seconds |
| Broker → device | `esp32/led` | `ON` or `OFF` | Sets the LED output high or low |

Payload matching is case-sensitive in the current sketch.

## Architecture

```text
Simulated Sensor Task
        |
        | integer value
        v
   FreeRTOS Queue ------> LED Task ------> GPIO 2
                              ^
                              |
Button GPIO 4 --> ISR --> Binary Semaphore

MQTT Task <---- Wi-Fi / MQTT broker
    |                         |
    | publishes                | command on esp32/led
    v                         v
esp32/sensor              callback sets LED
```

## Known limitations and next improvements

These are important follow-up items before treating this as a reliable system:

1. **MQTT client is accessed from multiple tasks.** The sensor task calls `client.publish()` while the MQTT task calls `client.loop()` and manages the connection. The current sketch does not serialize these operations. Refactor to make one MQTT-owning task handle publish requests from a queue before relying on this under load.
2. **Publishing can happen before MQTT is connected.** The sensor task does not check `client.connected()` and ignores the return value from `publish()`; messages may be dropped during startup or reconnects.
3. **Reconnect loop can block indefinitely.** `reconnect()` retries without a bounded backoff or a visible error state.
4. **Queue sends can block.** `xQueueSend(..., portMAX_DELAY)` can block the sensor task if the queue fills. Choose and document a queue-full policy for the intended workload.
5. **Button input needs validation.** The ISR has no explicit debounce strategy. Test switch bounce and consider a timer or task-level debounce.
6. **LED behavior is intentionally simple.** Sensor values at or below 30 do not turn the LED off; the button and MQTT callback can also change the same output. Define one clear state owner before extending the demo.
7. **Simulated data only.** Replace `random(20, 40)` with a real sensor read only after documenting the sensor, wiring, units, and error handling.

## Verification checklist

- [ ] Sketch compiles with the selected Arduino-ESP32 core and installed PubSubClient version.
- [ ] Board and GPIO assignments have been checked against the actual hardware.
- [ ] Serial output confirms Wi-Fi and MQTT connection (add diagnostics before relying on this).
- [ ] MQTT command handling has been tested with `ON` and `OFF`.
- [ ] Sensor topic behavior has been checked during broker disconnect/reconnect.
- [ ] Button bounce and queue-full behavior have been tested.
- [ ] No real Wi-Fi credentials or broker secrets are committed.

## Contribution workflow

Use a focused branch for each change. Include the board/core/library versions, reproduction steps, expected behavior, observed result, and test evidence in pull requests. Do not describe hardware tests as passed unless they were actually run.

---

**License:** No license is specified in this repository yet. Ask the maintainer to choose a license before redistributing or reusing this project.
