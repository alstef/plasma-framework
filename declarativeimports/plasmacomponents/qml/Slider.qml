/*
*   Copyright (C) 2011 by Daker Fernandes Pinheiro <dakerfp@gmail.com>
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU Library General Public License as
*   published by the Free Software Foundation; either version 2, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details
*
*   You should have received a copy of the GNU Library General Public
*   License along with this program; if not, write to the
*   Free Software Foundation, Inc.,
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

import QtQuick 1.0
import org.kde.plasma.core 0.1 as PlasmaCore

// TODO: create a value indicator for plasma?
Item {
    id: slider

    // Common API
    property alias stepSize: range.stepSize
    property alias minimumValue: range.minimumValue
    property alias maximumValue: range.maximumValue
    property alias value: range.value
    property int orientation: Qt.Horizontal
    property alias pressed: mouseArea.pressed
    property bool valueIndicatorVisible: false
    property string valueIndicatorText: value

    // Plasma API
    property bool animated: false
    property alias inverted: range.inverted
    property bool updateValueWhileDragging: true
    property real handleSize: 22

    // Convinience API
    property bool _isVertical: orientation == Qt.Vertical

    width: _isVertical ? 22 : 200
    height: _isVertical ? 200 : 22
    // TODO: needs to define if there will be specific graphics for
    //     disabled sliders
    opacity: enabled ? 1.0 : 0.5

    Keys.onUpPressed: {
        if (!enabled || !_isVertical)
            return;

        if (inverted)
            value -= stepSize;
        else
            value += stepSize;
    }

    Keys.onDownPressed: {
        if (!enabled || !enabled)
            return;

        if (!_isVertical)
            return;

        if (inverted)
            value += stepSize;
        else
            value -= stepSize;
    }

    Keys.onLeftPressed: {
        if (!enabled || _isVertical)
            return;

        if (inverted)
            value += stepSize;
        else
            value -= stepSize;
    }

    Keys.onRightPressed: {
        if (!enabled || _isVertical)
            return;

        if (inverted)
            value -= stepSize;
        else
            value += stepSize;
    }

    Item {
        id: contents

        width: _isVertical ? slider.height : slider.width
        height: _isVertical ? slider.width : slider.height
        rotation: _isVertical ? -90 : 0

        anchors.centerIn: parent

        RangeModel {
            id: range

            minimumValue: 0.0
            maximumValue: 1.0
            value: 0
            stepSize: 0.0
            inverted: false
            positionAtMinimum: 0 + handle.width / 2
            positionAtMaximum: contents.width - handle.width / 2
        }

        PlasmaCore.SvgItem {
            id: groove

            anchors {
                left: parent.left
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            height: 3
            svg: PlasmaCore.Svg { imagePath: "widgets/slider" }
            elementId: "horizontal-slider-line"
        }

        PlasmaCore.SvgItem {
            id: focus

            transform: Translate { x: - handle.width / 2 }
            anchors {
                fill: handle
                margins: -1 // Hardcoded
            }
            opacity: slider.activeFocus ? 1 : 0
            svg: PlasmaCore.Svg { imagePath: "widgets/slider" }
            elementId: "horizontal-slider-hover"

            Behavior on opacity {
                PropertyAnimation { duration: 250 }
            }
        }

        PlasmaCore.SvgItem {
            id: hover

            transform: Translate { x: - handle.width / 2 }
            anchors {
                fill: handle
                margins: -2 // Hardcoded
            }
            opacity: 0
            svg: PlasmaCore.Svg { imagePath: "widgets/slider" }
            elementId: "horizontal-slider-hover"

            Behavior on opacity {
                PropertyAnimation { duration: 250 }
            }
        }

        PlasmaCore.SvgItem {
            id: handle

            transform: Translate { x: - handle.width / 2 }
            x: fakeHandle.x
            anchors {
                verticalCenter: groove.verticalCenter
            }
            width: handleSize
            height: handleSize
            svg: PlasmaCore.Svg { imagePath: "widgets/slider" }
            elementId: "horizontal-slider-handle"

            Behavior on x {
                id: behavior
                enabled: !mouseArea.drag.active && slider.animated

                PropertyAnimation {
                    duration: behavior.enabled ? 150 : 0
                    easing.type: Easing.OutSine
                }
            }
        }

        Item {
            id: fakeHandle
            width: handle.width
            height: handle.height
            transform: Translate { x: - handle.width / 2 }
        }

        MouseArea {
            id: mouseArea

            anchors.fill: parent
            enabled: slider.enabled
            drag {
                target: fakeHandle
                axis: Drag.XAxis
                minimumX: range.positionAtMinimum
                maximumX: range.positionAtMaximum
            }
            hoverEnabled: true

            onEntered: {
                hover.opacity = 1;
            }
            onExited: {
                if (!pressed)
                    hover.opacity = 0;
            }
            onPressed: {
                // Clamp the value
                var newX = Math.max(mouse.x, drag.minimumX);
                newX = Math.min(newX, drag.maximumX);

                // Debounce the press: a press event inside the handler will not
                // change its position, the user needs to drag it.
                if (Math.abs(newX - fakeHandle.x) > handle.width / 2)
                    range.position = newX;

                slider.forceActiveFocus();
            }
            onReleased: {
                // If we don't update while dragging, this is the only
                // moment that the range is updated.
                if (!slider.updateValueWhileDragging)
                    range.position = fakeHandle.x;

                if (!containsMouse)
                    hover.opacity = 0;
            }
        }
    }

    // Range position normally follow fakeHandle, except when
    // 'updateValueWhileDragging' is false. In this case it will only follow
    // if the user is not pressing the handle.
    Binding {
        when: updateValueWhileDragging || !mouseArea.pressed
        target: range
        property: "position"
        value: fakeHandle.x
    }

    // During the drag, we simply ignore position set from the range, this
    // means that setting a value while dragging will not "interrupt" the
    // dragging activity.
    Binding {
        when: !mouseArea.drag.active
        target: fakeHandle
        property: "x"
        value: range.position
    }
}