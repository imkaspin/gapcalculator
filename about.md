# Gap Calculator

Select any two objects in the editor, press the **Gap** button, and get:

- The edge-to-edge gap on the X and Y axes (in units and blocks)
- The **minimum TPS** needed to get at least one physics tick inside the gap
- The **recommended TPS** for a safe number of ticks inside the gap
- How many ticks fit inside the gap at the default 240 TPS

If the gap is smaller than the player hitbox, the popup shows the maximum TPS at which the player could clip across it in a single tick.

## Settings

- **Speed portal** (0.5x to 4x) or a **custom speed** in units/sec
- **Player size** (normal 30u or mini 18u)
- **Gap axis** (auto, horizontal or vertical)
- **Safety ticks** used for the recommended TPS
