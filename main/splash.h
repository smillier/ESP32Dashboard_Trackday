#pragma once

// Power-on animation: checkered-flag strips, speed streaks, the product name
// and version in a skewed racing style, and a five-light start sequence. It
// runs on the LVGL loop (non-blocking) and fades into the dashboard when the
// lights go out. Call after ui_create(), from the LVGL task.
void splash_run(void);
