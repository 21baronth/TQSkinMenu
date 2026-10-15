#pragma once

class InputCaptureGate {
	bool _drain = false;
public:
	bool Capture(bool menuOpen, bool controlsHeld) {
		if (menuOpen) { this->_drain = true; return true; }
		if (!this->_drain) return false;
		// Consume the final release events as well as any held closing click/key.
		if (!controlsHeld) this->_drain = false;
		return true;
	}
};

template<class SetDisabled, class ProcessInput>
void ProcessWithInputBlocked(bool wasDisabled, SetDisabled setDisabled, ProcessInput process) {
	struct Restore {
		bool value;
		SetDisabled& setter;
		~Restore() { this->setter(this->value); }
	} restore{wasDisabled, setDisabled};
	setDisabled(true);
	// Still poll/drain the device so buffered menu clicks cannot replay on close.
	process();
}
