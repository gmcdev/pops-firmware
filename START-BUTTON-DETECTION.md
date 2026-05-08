# Start Button Detection — Research Notes

## Machine

Williams System 11 (specific game TBD)

## Goal

Detect the customer pressing the start button so the relay can pulse at exactly that moment, adding a credit to the machine. The intent was to gate the credit on a confirmed human intention to play.

## What We Tried

### Phase 1 — Opto-isolated row/column monitoring

Wired two PC817/PS2501 optocouplers:
- Column opto: Machine 5V → [series R] → Pin 1 → LED → Pin 2 → column wire → Pin 3 → GND → Pin 4 → GPIO 25
- Row opto: Machine 5V → [series R] → Pin 1 → LED → Pin 2 → row wire → Pin 3 → GND → Pin 4 → GPIO 27

Triggered an interrupt on the column opto (FALLING edge = column scan active). Sampled the row opto inside the ISR at various delays (0μs, 50μs, 75μs, 100μs, 150μs, 200μs, 300μs, 400μs) to find a timing window where the row clearly differentiated button-pressed from button-not-pressed.

**Finding**: The row opto always read LOW for nearly the entire column scan period, regardless of button state. Varying the series resistor (470Ω, 1kΩ, 2.2kΩ, 4.7kΩ, 5.6kΩ, 10kΩ) and the sample delay could not produce a clean separation. Root causes:

1. **Row strobe**: The System 11 6809 CPU briefly drives all row lines LOW through a 1kΩ series resistor at the start of each column scan (a standard PIA read-reset technique). With a 560Ω row pullup and 1kΩ series to the PIA output, the row wire settles to 3.21V during the strobe — enough to forward-bias the opto LED and trigger false positives.
2. **Capacitive coupling**: A fast column wire transition (5V → 0V) induces a coupled spike on the adjacent row wire. At t=0 of each scan, this pulls the row to ~0.66V regardless of button state.
3. **No clean timing window**: The strobe lasts the full column scan period. The column scan itself is only ~5–10μs. There is no moment within the scan where the row is clearly HIGH (no button) versus clearly LOW (button pressed) through an opto circuit — both conditions produce nearly identical opto currents.

### Phase 2 — Direct ADC approach (shared ground)

Removed both optos. Powered the ESP32 from the machine's 5V supply (shared ground). Wired the row wire directly to an ADC input (GPIO 34) and the column wire to a digital interrupt input (GPIO 25), both through 33kΩ/47kΩ voltage dividers.

With shared ground and direct ADC measurement, the plan was:
- Coupling spike at column FALLING edge: row at ~0.66V → ADC ~409
- Button press + column active: row at ~0.2V → ADC ~123
- Threshold of 200 would cleanly separate them

**Finding**: `isrRaw` was consistently ~409 whether the button was pressed or not. The ADC reading did not change with button state. The button press adds no additional row voltage drop beyond the coupling spike.

## Root Cause — The Series Diode

Williams System 11 uses a **series diode on each switch** for ghost-detection prevention. The diode is oriented so that when the column is driven LOW and the switch is closed, current flows:

```
Row wire (5V via 560Ω pullup) → Switch contacts → Diode (anode→cathode) → Column wire (0V)
```

The diode introduces a **~0.7V forward voltage drop**. When the button is pressed and the column is active:

```
V_row = V_column + V_diode = 0V + 0.7V = 0.7V
```

The **capacitive coupling spike** (no button press) pulls the row to **~0.66V** at t=0 of each column scan.

| Condition | Row wire voltage | ADC reading (GPIO 34) |
|---|---|---|
| Idle (column between scans) | ~5.0V | ~3543 |
| Column active, button NOT pressed (coupling) | ~0.66V | ~409 |
| Column active, button PRESSED (switch + diode) | ~0.70V | ~432 |

The **16–23 ADC count difference** (409 vs 432) is below the noise floor and fully within ADC nonlinearity. It is not reliably detectable.

## Why This Cannot Be Solved in Software

The fundamental problem is electrical, not timing or software:

1. **Coupling and switch closure produce the same voltage**. Both pull the row to ~0.66–0.70V during the column scan. This is a direct consequence of the diode's forward voltage drop being approximately equal to the capacitive coupling magnitude.

2. **The column scan is ~5–10μs**. This is shorter than the ESP32 ADC conversion time (~5μs), making time-domain separation nearly impossible. There is no sample point where the coupling has decayed but the switch closure is still visible.

3. **The diode is not accessible as a separate circuit node**. The diode is internal to the switch assembly. We cannot measure the column-side of the diode directly without physical modification to the switch wiring inside the machine.

4. **Increasing ADC resolution doesn't help**. Even if the ESP32 ADC were 16-bit, the signals (0.66V vs 0.70V) are physically too similar to distinguish reliably across temperature variation, supply noise, and machine-to-machine variation.

## What Would Be Required to Solve This

- **Oscilloscope measurement** of the exact diode forward voltage and coupling spike amplitude to verify the overlap.
- **Access to the column-side of the diode** (between the diode cathode and the column wire), which would show ~0V when the button is pressed vs ~0.66V from coupling alone. This would require splicing into the switch harness or adding a test point on the MPU board.
- **Alternatively**: monitor a machine output signal that changes when the game starts — the ball launch solenoid, the trough solenoid, or a display segment. These are definitive "game started" events, not subject to the matrix timing problem.

## Open Questions / Next Steps

The series diode hypothesis has not been physically confirmed — the machine model and service manual have not been consulted. Before concluding this is unsolvable, the following should be investigated:

1. **Confirm the diode**: Get the service manual for the specific machine. Verify whether the start button switch has a series diode and what its orientation is. If there is no diode, the 0.66V vs 0.20V signals ARE distinguishable and the approach can work with a threshold of ~300 ADC counts.

2. **Oscilloscope measurement**: Probe the row wire and column wire simultaneously with a scope during a button press. This would definitively show the voltage waveforms and timing, resolving the ambiguity that ADC polling alone cannot.

3. **Alternative physical access**: If a diode is confirmed, tap the signal from the column-side of the diode (between the diode cathode and the column wire). This point would read ~0V when the button is pressed (not 0.7V), giving a distinguishable signal.

4. **Alternative detection**: Monitor a machine output that definitively signals game start — ball launch solenoid, trough solenoid, or a display segment. These bypass the switch matrix entirely.

**No decision has been made on how to proceed.** This document records the investigation findings only.
