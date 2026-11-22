# Performance monitor revision

## Selectable styles and presets

The Performance page has an independent Monitor style selector: RTSS dark, Classic Win98, Green phosphor, Neon purple, and Minimal light. It updates both grouped overlays and free-position cards immediately and saves to the current preset. The application-wide theme is unchanged.

Eight editable slots are available: the original Custom 1–3, Win98/Gaming, Phosphor/FPS, Neon/All metrics, Light/Work, and Dark/Network and FPS. New slots supply initial metric selections, font size, columns and style; saved values take precedence. Switching slots saves the outgoing configuration. Old configurations without Style default to RTSS dark.

Reference: https://www.guru3d.com/download/rtss-rivatuner-statistics-server-download/

Grouped overlay: dark background, category colors, monospaced font, latest 240 chronological PresentMon frame intervals. Dashed reference is 16.667 ms (60 FPS). Vertical scale expands for spikes. Existing five-second FPS and slowest-one-percent statistics remain unchanged. Free-position cards retain their previous rendering, without a chart.

Select a process, enable FPS collection, and include FPS in the displayed items. Toggle with Ctrl+Shift+F7. Existing presets retain their selected metrics; new presets include CPU, GPU, RAM, VRAM and FPS.

This remains a desktop overlay, without RTSS injection or a frame limiter. Exclusive fullscreen compatibility is not guaranteed. Rendering is bounded to 240 points; reduced overhead has not been benchmarked.

Validation: build and link succeeded; options migration and performance aggregation tests passed. Live game collection and visual DPI regression remain unverified.
