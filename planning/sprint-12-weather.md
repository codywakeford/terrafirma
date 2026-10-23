# Sprint 12 — Weather on the grid (plan, not started)

Goal: weather that moves. A coarse atmosphere on terragen's grid, stepped
in game time from the climate it already has, so fronts, rain, snow and
storms travel across the world instead of being a per-column constant.

## Decisions (proposed, for Cody to confirm)

1. **Atmosphere on the coarse grid, server side.** Per cell: air
   temperature, humidity, pressure anomaly, cloud and precipitation. Stepped
   every few game minutes; deterministic from the world code and the clock.
2. **Driven by what exists:** the date's sunlight (sprint 3), prevailing
   winds and rain shadows (terragen climate), sea vs land heat capacity.
   Weather is a departure from climate that relaxes back to it.
3. **Advection, not fluid dynamics.** Humidity and heat move with the
   wind; lifting over mountains and convergence make cloud and rain.
   Pressure anomalies drift east in the mid-latitudes (fronts).
4. **Events as thresholds, data over branches:** thunderstorms (warm, wet,
   unstable), blizzards (cold, wet, windy), droughts (long dry spell),
   volcanic ash from active volcanoes (sprint 9) cooling the cells downwind.
5. **`core.terra.weather(pos)`** for game Lua; the client gets the local
   cell for rain, snow and sky colour.
6. **A Lab scenario:** a strip of cells with a mountain, watch a rain shadow
   form.
