/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "gui/message.h"
#include "common/translation.h"

#include "backends/platform/ios7/ios7_osys_main.h"

static const int kQueuedInputEventDelay = 50;

bool OSystem_iOS7::pollEvent(Common::Event &event) {
	//printf("pollEvent()\n");

	long curTime = getMillis();

	if (_timerCallback && (curTime >= _timerCallbackNext)) {
		_timerCallback(_timerCallbackTimer);
		_timerCallbackNext = curTime + _timerCallbackTimer;
	}

	if (_queuedInputEvent.type != Common::EVENT_INVALID && curTime >= _queuedEventTime) {
		event = _queuedInputEvent;
		_queuedInputEvent.type = Common::EVENT_INVALID;
		return true;
	}

	InternalEvent internalEvent;

	if (iOS7_fetchEvent(&internalEvent)) {
		switch (internalEvent.type) {
		case kInputMouseDown:
			if (!handleEvent_mouseDown(event, internalEvent.value1, internalEvent.value2))
				return false;
			break;

		case kInputMouseUp:
			if (!handleEvent_mouseUp(event, internalEvent.value1, internalEvent.value2))
				return false;
			break;

		case kInputMouseDragged:
			if (!handleEvent_mouseDragged(event, internalEvent.value1, internalEvent.value2))
				return false;
			break;

		case kInputOrientationChanged:
			handleEvent_orientationChanged(internalEvent.value1);
			return false;

		case kInputApplicationSuspended:
			handleEvent_applicationSuspended();
			return false;

		case kInputApplicationResumed:
			handleEvent_applicationResumed();
			return false;

		case kInputApplicationSaveState:
			handleEvent_applicationSaveState();
			return false;

		case kInputApplicationRestoreState:
			handleEvent_applicationRestoreState();
			return false;

		case kInputApplicationClearState:
			handleEvent_applicationClearState();
			return false;

		case kInputMouseSecondDragged:
			if (!handleEvent_mouseSecondDragged(event, internalEvent.value1, internalEvent.value2))
				return false;
			break;
		case kInputMouseSecondDown:
			_secondaryTapped = true;
			if (!handleEvent_secondMouseDown(event, internalEvent.value1, internalEvent.value2))
				return false;
			break;
		case kInputMouseSecondUp:
			_secondaryTapped = false;
			if (!handleEvent_secondMouseUp(event, internalEvent.value1, internalEvent.value2))
				return false;
			break;

		case kInputKeyPressed:
			handleEvent_keyPressed(event, internalEvent.value1);
			break;

		case kInputSwipe:
			if (!handleEvent_swipe(event, internalEvent.value1, internalEvent.value2))
				return false;
			break;

		case kInputTap:
			if (!handleEvent_tap(event, (UIViewTapDescription) internalEvent.value1, internalEvent.value2))
				return false;
			break;

		case kInputMainMenu:
			event.type = Common::EVENT_MAINMENU;
			_queuedInputEvent.type = Common::EVENT_INVALID;
			_queuedEventTime = getMillis() + kQueuedInputEventDelay;
			break;

		case kInputJoystickAxisMotion:
			event.type = Common::EVENT_JOYAXIS_MOTION;
			event.joystick.axis = internalEvent.value1;
			event.joystick.position = internalEvent.value2;
			break;

		case kInputJoystickButtonDown:
			event.type = Common::EVENT_JOYBUTTON_DOWN;
			event.joystick.button = internalEvent.value1;
			break;

		case kInputJoystickButtonUp:
			event.type = Common::EVENT_JOYBUTTON_UP;
			event.joystick.button = internalEvent.value1;

		case kInputChanged:
			event.type = Common::EVENT_INPUT_CHANGED;
			_queuedInputEvent.type = Common::EVENT_INVALID;
			_queuedEventTime = getMillis() + kQueuedInputEventDelay;
			break;

		default:
			break;
		}

		return true;
	}
	return false;
}

bool OSystem_iOS7::handleEvent_mouseDown(Common::Event &event, int x, int y) {
	//printf("Mouse down at (%u, %u)\n", x, y);

	// Workaround: kInputMouseSecondToggled isn't always sent when the
	// secondary finger is lifted. Need to make sure we get out of that mode.
	_secondaryTapped = false;

	if (_touchpadModeEnabled) {
		_lastPadX = x;
		_lastPadY = y;
	} else
		warpMouse(x, y);

	if (_mouseClickAndDragEnabled) {
		event.type = Common::EVENT_LBUTTONDOWN;
		event.mouse.x = _videoContext->mouseX;
		event.mouse.y = _videoContext->mouseY;
		return true;
	} else {
		_lastMouseDown = getMillis();
	}
	return false;
}

bool OSystem_iOS7::handleEvent_mouseUp(Common::Event &event, int x, int y) {
	//printf("Mouse up at (%u, %u)\n", x, y);

	if (_secondaryTapped) {
		_secondaryTapped = false;
		if (!handleEvent_secondMouseUp(event, x, y))
			return false;
	} else if (_mouseClickAndDragEnabled) {
		event.type = Common::EVENT_LBUTTONUP;
		event.mouse.x = _videoContext->mouseX;
		event.mouse.y = _videoContext->mouseY;
	} else {
		if (getMillis() - _lastMouseDown < 250) {
			event.type = Common::EVENT_LBUTTONDOWN;
			event.mouse.x = _videoContext->mouseX;
			event.mouse.y = _videoContext->mouseY;

			_queuedInputEvent.type = Common::EVENT_LBUTTONUP;
			_queuedInputEvent.mouse.x = _videoContext->mouseX;
			_queuedInputEvent.mouse.y = _videoContext->mouseY;
			_lastMouseTap = getMillis();
			_queuedEventTime = _lastMouseTap + kQueuedInputEventDelay;
		} else
			return false;
	}

	return true;
}

bool OSystem_iOS7::handleEvent_secondMouseDown(Common::Event &event, int x, int y) {
	_lastSecondaryDown = getMillis();
	_gestureStartX = x;
	_gestureStartY = y;

	if (_mouseClickAndDragEnabled) {
		event.type = Common::EVENT_LBUTTONUP;
		event.mouse.x = _videoContext->mouseX;
		event.mouse.y = _videoContext->mouseY;

		_queuedInputEvent.type = Common::EVENT_RBUTTONDOWN;
		_queuedInputEvent.mouse.x = _videoContext->mouseX;
		_queuedInputEvent.mouse.y = _videoContext->mouseY;
	} else
		return false;

	return true;
}

bool OSystem_iOS7::handleEvent_secondMouseUp(Common::Event &event, int x, int y) {
	int curTime = getMillis();

	if (curTime - _lastSecondaryDown < 400) {
		//printf("Right tap!\n");
		if (curTime - _lastSecondaryTap < 400 && !_videoContext->overlayVisible) {
			//printf("Right escape!\n");
			event.type = Common::EVENT_KEYDOWN;
			_queuedInputEvent.type = Common::EVENT_KEYUP;

			event.kbd.flags = _queuedInputEvent.kbd.flags = 0;
			event.kbd.keycode = _queuedInputEvent.kbd.keycode = Common::KEYCODE_ESCAPE;
			event.kbd.ascii = _queuedInputEvent.kbd.ascii = Common::ASCII_ESCAPE;
			_queuedEventTime = curTime + kQueuedInputEventDelay;
			_lastSecondaryTap = 0;
		} else if (!_mouseClickAndDragEnabled) {
			//printf("Rightclick!\n");
			event.type = Common::EVENT_RBUTTONDOWN;
			event.mouse.x = _videoContext->mouseX;
			event.mouse.y = _videoContext->mouseY;
			_queuedInputEvent.type = Common::EVENT_RBUTTONUP;
			_queuedInputEvent.mouse.x = _videoContext->mouseX;
			_queuedInputEvent.mouse.y = _videoContext->mouseY;
			_lastSecondaryTap = curTime;
			_queuedEventTime = curTime + kQueuedInputEventDelay;
		} else {
			//printf("Right nothing!\n");
			return false;
		}
	}
	if (_mouseClickAndDragEnabled) {
		event.type = Common::EVENT_RBUTTONUP;
		event.mouse.x = _videoContext->mouseX;
		event.mouse.y = _videoContext->mouseY;
	}

	return true;
}

bool OSystem_iOS7::handleEvent_mouseDragged(Common::Event &event, int x, int y) {
	if (_lastDragPosX == x && _lastDragPosY == y)
		return false;

	_lastDragPosX = x;
	_lastDragPosY = y;

	//printf("Mouse dragged at (%u, %u)\n", x, y);
	int mouseNewPosX;
	int mouseNewPosY;
	if (_touchpadModeEnabled) {
		int deltaX = _lastPadX - x;
		int deltaY = _lastPadY - y;
		_lastPadX = x;
		_lastPadY = y;

		mouseNewPosX = (int)(_videoContext->mouseX - deltaX / 0.5f);
		mouseNewPosY = (int)(_videoContext->mouseY - deltaY / 0.5f);

		int widthCap = _videoContext->overlayVisible ? _videoContext->overlayWidth : _videoContext->screenWidth;
		int heightCap = _videoContext->overlayVisible ? _videoContext->overlayHeight : _videoContext->screenHeight;

		if (mouseNewPosX < 0)
			mouseNewPosX = 0;
		else if (mouseNewPosX > widthCap)
			mouseNewPosX = widthCap;

		if (mouseNewPosY < 0)
			mouseNewPosY = 0;
		else if (mouseNewPosY > heightCap)
			mouseNewPosY = heightCap;

	} else {
		mouseNewPosX = x;
		mouseNewPosY = y;
	}

	event.type = Common::EVENT_MOUSEMOVE;
	event.mouse.x = mouseNewPosX;
	event.mouse.y = mouseNewPosY;
	warpMouse(mouseNewPosX, mouseNewPosY);

	return true;
}

bool OSystem_iOS7::handleEvent_mouseSecondDragged(Common::Event &event, int x, int y) {
	if (_gestureStartX == -1 || _gestureStartY == -1) {
		return false;
	}

	static const int kNeededLength = 100;
	static const int kMaxDeviation = 20;

	int vecX = (x - _gestureStartX);
	int vecY = (y - _gestureStartY);

	int absX = abs(vecX);
	int absY = abs(vecY);

	//printf("(%d, %d)\n", vecX, vecY);

	if (absX >= kNeededLength || absY >= kNeededLength) { // Long enough gesture to react upon.
		_gestureStartX = -1;
		_gestureStartY = -1;

		if (absX < kMaxDeviation && vecY >= kNeededLength) {
			// Swipe down
			event.type = Common::EVENT_MAINMENU;
			_queuedInputEvent.type = Common::EVENT_INVALID;

			_queuedEventTime = getMillis() + kQueuedInputEventDelay;
			return true;
		}

		if (absX < kMaxDeviation && -vecY >= kNeededLength) {
			// Swipe up
			_mouseClickAndDragEnabled = !_mouseClickAndDragEnabled;
			Common::U32String dialogMsg;
			if (_mouseClickAndDragEnabled) {
				_touchpadModeEnabled = false;
				dialogMsg = _("Mouse-click-and-drag mode enabled.");
			} else
				dialogMsg = _("Mouse-click-and-drag mode disabled.");
			GUI::TimedMessageDialog dialog(dialogMsg, 1500);
			dialog.runModal();
			return false;
		}

		if (absY < kMaxDeviation && vecX >= kNeededLength) {
			// Swipe right
			_touchpadModeEnabled = !_touchpadModeEnabled;
			Common::U32String dialogMsg;
			if (_touchpadModeEnabled)
				dialogMsg = _("Touchpad mode enabled.");
			else
				dialogMsg = _("Touchpad mode disabled.");
			GUI::TimedMessageDialog dialog(dialogMsg, 1500);
			dialog.runModal();
			return false;

		}

		if (absY < kMaxDeviation && -vecX >= kNeededLength) {
			// Swipe left
			return false;
		}
	}

	return false;
}

void  OSystem_iOS7::handleEvent_orientationChanged(int orientation) {
	//printf("Orientation: %i\n", orientation);

	ScreenOrientation newOrientation;
	switch (orientation) {
	case 1:
		newOrientation = kScreenOrientationPortrait;
		break;
	case 2:
		newOrientation = kScreenOrientationFlippedPortrait;
		break;
	case 3:
		newOrientation = kScreenOrientationLandscape;
		break;
	case 4:
		newOrientation = kScreenOrientationFlippedLandscape;
		break;
	default:
		return;
	}

	if (_screenOrientation != newOrientation) {
		_screenOrientation = newOrientation;
		rebuildSurface();
	}
}

void OSystem_iOS7::rebuildSurface() {
	updateOutputSurface();

	dirtyFullScreen();
	if (_videoContext->overlayVisible) {
			dirtyFullOverlayScreen();
		}
	updateScreen();
}

void OSystem_iOS7::handleEvent_applicationSuspended() {
	suspendLoop();
}

void OSystem_iOS7::handleEvent_applicationResumed() {
	rebuildSurface();
}

void  OSystem_iOS7::handleEvent_keyPressed(Common::Event &event, int keyPressed) {
	int ascii = keyPressed;
	//printf("key: %i\n", keyPressed);

	// Map LF character to Return key/CR character
	if (keyPressed == 10) {
		keyPressed = Common::KEYCODE_RETURN;
		ascii = Common::ASCII_RETURN;
	}

	event.type = Common::EVENT_KEYDOWN;
	_queuedInputEvent.type = Common::EVENT_KEYUP;

	event.kbd.flags = _queuedInputEvent.kbd.flags = 0;
	event.kbd.keycode = _queuedInputEvent.kbd.keycode = (Common::KeyCode)keyPressed;
	event.kbd.ascii = _queuedInputEvent.kbd.ascii = ascii;
	_queuedEventTime = getMillis() + kQueuedInputEventDelay;
}

bool OSystem_iOS7::handleEvent_swipe(Common::Event &event, int direction, int touches) {
	if (touches == 3) {
		Common::KeyCode keycode = Common::KEYCODE_INVALID;
		switch ((UIViewSwipeDirection)direction) {
			case kUIViewSwipeUp:
				keycode = Common::KEYCODE_UP;
				break;
			case kUIViewSwipeDown:
				keycode = Common::KEYCODE_DOWN;
				break;
			case kUIViewSwipeLeft:
				keycode = Common::KEYCODE_LEFT;
				break;
			case kUIViewSwipeRight:
				keycode = Common::KEYCODE_RIGHT;
				break;
			default:
				return false;
		}

		event.kbd.keycode = _queuedInputEvent.kbd.keycode = keycode;
		event.kbd.ascii = _queuedInputEvent.kbd.ascii = 0;
		event.type = Common::EVENT_KEYDOWN;
		_queuedInputEvent.type = Common::EVENT_KEYUP;
		event.kbd.flags = _queuedInputEvent.kbd.flags = 0;
		_queuedEventTime = getMillis() + kQueuedInputEventDelay;

		return true;
	}
	else if (touches == 2) {
		switch ((UIViewSwipeDirection)direction) {
		case kUIViewSwipeUp: {
			_mouseClickAndDragEnabled = !_mouseClickAndDragEnabled;
			Common::U32String dialogMsg;
			if (_mouseClickAndDragEnabled) {
				_touchpadModeEnabled = false;
				dialogMsg = _("Mouse-click-and-drag mode enabled.");
			} else
				dialogMsg = _("Mouse-click-and-drag mode disabled.");
			GUI::TimedMessageDialog dialog(dialogMsg, 1500);
			dialog.runModal();
			return false;
		}

		case kUIViewSwipeDown: {
			// Swipe down
			event.type = Common::EVENT_MAINMENU;
			_queuedInputEvent.type = Common::EVENT_INVALID;
			_queuedEventTime = getMillis() + kQueuedInputEventDelay;
			return true;
		}

		case kUIViewSwipeRight: {
			// Swipe right
			_touchpadModeEnabled = !_touchpadModeEnabled;
			Common::U32String dialogMsg;
			if (_touchpadModeEnabled)
				dialogMsg = _("Touchpad mode enabled.");
			else
				dialogMsg = _("Touchpad mode disabled.");
			GUI::TimedMessageDialog dialog(dialogMsg, 1500);
			dialog.runModal();
			return false;
		}

		default:
			break;
		}
	}
	return false;
}

bool OSystem_iOS7::handleEvent_tap(Common::Event &event, UIViewTapDescription type, int touches) {
	if (touches == 1) {
		if (type == kUIViewTapDouble) {
			event.type = Common::EVENT_RBUTTONDOWN;
			_queuedInputEvent.type = Common::EVENT_RBUTTONUP;
			_queuedEventTime = getMillis() + kQueuedInputEventDelay;
			return true;
		}
	}
	else if (touches == 2) {
		if (type == kUIViewTapDouble) {
			event.kbd.keycode = _queuedInputEvent.kbd.keycode = Common::KEYCODE_ESCAPE;
			event.kbd.ascii = _queuedInputEvent.kbd.ascii = Common::ASCII_ESCAPE;
			event.type = Common::EVENT_KEYDOWN;
			_queuedInputEvent.type = Common::EVENT_KEYUP;
			event.kbd.flags = _queuedInputEvent.kbd.flags = 0;
			_queuedEventTime = getMillis() + kQueuedInputEventDelay;
			return true;
		}
	}
	return false;
}
