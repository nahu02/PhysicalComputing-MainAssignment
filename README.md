# Tactile Simon Says

The communication uses I2C with one ESP32 as the main, and ATMEGA32s as agents.

## Main (ESP32 C3)

The main acts as the game master, coordinating the game through I2C broadcast messages.

### Game Flow

1. **Setup**: Initialize I2C as main, generate random pattern (3-6 elements from global indexes)
2. **Phase 1**: Broadcast pattern sequence to all agents
3. **Phase 2**: Poll agents for their button input sequence
4. **Phase 3**: Validate and report WIN/LOSE via Serial

### Pattern Generation

- Pattern length: Random 3-6 elements
- Pattern values: Random selection from `GLOBAL_INDEXES` (1, 2, 3, 4)
- Uses ESP32 hardware RNG (no explicit seed)

## I2C Communication Protocol

**I2C Address**: Broadcast to `0x00` (all agents receive)

### Message Types

All messages are single-byte values transmitted over I2C.

| Byte Value | Type | Description |
|------------|------|-------------|
| `0x00` | Stop Signal | Deactivates current actuator |
| `0xF1` | Phase 1 Start | Signals phase 1 beginning |
| `0xF2` | Phase 2 Start | Signals agents to accept input |
| `0x11` - `0xFF` | Actuator Activation | Activates actuator at global index (tens=category, ones=index 1-F) |

**Actuator Byte Format**: `0x[0-F][1-F]`
- High nibble: Category (0-F) 
- Low nibble: Global index (1-F, where 1-4 are used for this game)

Examples:
- `0x01` = Activate global index 1
- `0x02` = Activate global index 2
- `0x03` = Activate global index 3
- `0x04` = Activate global index 4

### Timing

- **Actuator Active**: 500ms (configurable via `ACTUATOR_ACTIVE_LENGTH_MS`). Defines how long an actuator remains active, e.g. how long a LED is lit or a vibration motor runs.
- **Inter-element Delay**: 300ms between stop signal and next element
- **Phase Transition**: 100ms after final stop signal

### Phase 2 Polling

In Phase 2, the main continuously polls each agent in the `I2C_AGENTS` array in a loop:

1. Main requests data from agent (I2C read)
2. Agent responds with:
   - The global index of a button press (if a button was pressed since last poll)
   - Nothing/empty (if no button press occurred)
3. Main compares received index against expected next element in `activePattern`
4. Continues until pattern is complete or mismatch occurs, then transitions to Phase 3

### Phase 3 Results

After Phase 2 completes, the main reports the game result via Serial:

**WIN**: All pattern elements matched in correct order 
**LOSE**: Any mismatch between received input and pattern

## Example Sequence

```
[Main → All] 0xF1              // Phase 1 start
[Main → All] 0x03              // Activate global index 3
[500ms delay]
[Main → All] 0x00              // Stop signal
[300ms delay]
[Main → All] 0x01              // Activate global index 1
[500ms delay]
[Main → All] 0x00              // Stop signal
[100ms delay]
[Main → All] 0xF2              // Phase 2 start
[Main polls agents...]
  [Main ← Agent1] 0x03         // Agent reports button 3 pressed
  [Main ← Agent2] 0x00         // Agent has nothing
  [Main ← Agent1] 0x01         // Agent reports button 1 pressed
  [Main ← Agent2] 0x00         // Agent has nothing
  ... continues until pattern complete or mismatch
```
