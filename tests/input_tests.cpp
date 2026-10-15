#include "../src/input_capture.h"
#include <cassert>
#include <stdexcept>
#include <iostream>

int main() {
	InputCaptureGate gate;
	assert(!gate.Capture(false, true)); // Normal gameplay is never blocked initially.
	assert(gate.Capture(true, false));
	assert(gate.Capture(false, true)); // Closing mouse/key remains captured.
	assert(gate.Capture(false, true));
	assert(gate.Capture(false, false)); // Drain its release event.
	assert(!gate.Capture(false, false));
	assert(gate.Capture(true, true)); // Reopen during a held control.
	assert(gate.Capture(false, false));
	assert(!gate.Capture(false, true));
	for (bool previous : {false, true}) {
		bool disabled = previous;
		int queuedClicks = 3, gameActions = 0;
		ProcessWithInputBlocked(previous, [&](bool value) { disabled = value; }, [&] {
			assert(disabled);
			if (!disabled) gameActions += queuedClicks;
			queuedClicks = 0;
		});
		assert(disabled == previous && queuedClicks == 0 && gameActions == 0);
		try {
			ProcessWithInputBlocked(previous, [&](bool value) { disabled = value; }, [] { throw std::runtime_error("input failure"); });
		} catch (const std::runtime_error&) {}
		assert(disabled == previous);
	}
	std::cout << "Passed input capture, release draining, and previous-state restoration.\n";
}
