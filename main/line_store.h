#pragma once

#include "telemetry.h"

// Persists the start/finish line in NVS so it survives power cycles.
namespace line_store {

bool load(telemetry::Line* line);
void save(const telemetry::Line& line);

} // namespace line_store
