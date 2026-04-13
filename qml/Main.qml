import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtWayland.Compositor 1.15

Rectangle {
    id: root
    color: "#002b36"

    readonly property bool hasActiveWorkspace: workspaceModel.activeIndex >= 0 && (workspaceModel.count - workspaceModel.pinnedCount) > 0

    WaylandMouseTracker {
        anchors.fill: parent
        windowSystemCursorEnabled: true

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

            // Left: workspace area with tab bar
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.hasActiveWorkspace
                spacing: 0

                // Tab bar (unpinned workspaces only)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: "#073642"
                    radius: 4

                    Row {
                        id: tabBar
                        anchors.fill: parent
                        anchors.margins: 2
                        spacing: 4

                        Repeater {
                            model: workspaceModel

                            Rectangle {
                                visible: !model.pinned
                                width: visible ? badgeText.implicitWidth + tabLabel.implicitWidth + 28 : 0
                                height: tabBar.height
                                color: index === workspaceModel.activeIndex ? "#2aa198" : "#002b36"
                                radius: 3

                                readonly property int unpinnedPos: workspaceModel.unpinnedPositionOf(index)

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 4

                                    Text {
                                        id: badgeText
                                        text: unpinnedPos >= 0 && unpinnedPos < 9 ? (unpinnedPos + 1).toString() : ""
                                        color: index === workspaceModel.activeIndex ? "#002b36" : "#586e75"
                                        font.family: "monospace"
                                        font.pixelSize: 10
                                        font.bold: true
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Text {
                                        id: tabLabel
                                        text: model.title
                                        color: index === workspaceModel.activeIndex ? "#002b36" : "#839496"
                                        font.family: "monospace"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: workspaceModel.activateWorkspace(index)
                                }
                            }
                        }
                    }
                }

                // Workspace surface area (single active, virtual desktop)
                Item {
                    id: workspaceArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    onWidthChanged: reportSize()
                    onHeightChanged: reportSize()
                    onVisibleChanged: reportSize()

                    function reportSize() {
                        if (visible && width > 0 && height > 0) {
                            compositor.setClientArea(Math.round(width), Math.round(height));
                        }
                    }

                    Repeater {
                        id: workspaceRepeater
                        model: workspaceModel

                        Item {
                            visible: !model.pinned && index === workspaceModel.activeIndex
                            anchors.fill: parent

                            ShellSurfaceItem {
                                id: surfaceItem
                                shellSurface: model.xdgSurface
                                autoCreatePopupItems: false
                                inputEventsEnabled: false
                                anchors.fill: parent
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                z: 1
                                onPressed: function(mouse) {
                                    compositor.forwardMousePress(
                                        surfaceItem, mouse.x, mouse.y, mouse.button)
                                }
                                onReleased: function(mouse) {
                                    compositor.forwardMouseRelease(mouse.button)
                                }
                                onPositionChanged: function(mouse) {
                                    compositor.forwardMouseMove(
                                        surfaceItem, mouse.x, mouse.y)
                                }
                            }
                        }
                    }
                }
            }

            // Right: chat sidebar (always visible)
            ColumnLayout {
                id: chatSidebar
                Layout.preferredWidth: root.hasActiveWorkspace ? 320 : -1
                Layout.fillWidth: !root.hasActiveWorkspace
                Layout.fillHeight: true
                spacing: 8

                ListView {
                    id: messageList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: chatModel
                    clip: true
                    spacing: 8

                    delegate: Rectangle {
                        width: messageList.width
                        height: msgText.implicitHeight + 16
                        color: model.role === "user" ? "#073642" : "#002b36"
                        radius: 6

                        Text {
                            id: msgText
                            anchors.fill: parent
                            anchors.margins: 8
                            text: model.content
                            color: model.role === "user" ? "#839496" : "#2aa198"
                            wrapMode: Text.Wrap
                            font.family: "monospace"
                            font.pixelSize: 14
                        }
                    }

                    onCountChanged: {
                        Qt.callLater(function() {
                            messageList.positionViewAtEnd();
                        });
                    }

                    Connections {
                        target: chatModel
                        function onDataChanged() {
                            Qt.callLater(function() {
                                messageList.positionViewAtEnd();
                            });
                        }
                    }
                }

                // Prompt + pinned surfaces side by side
                Row {
                    Layout.fillWidth: true
                    spacing: 8
                    layoutDirection: Qt.LeftToRight

                    Rectangle {
                        width: parent.width - pinnedRow.width - (pinnedRow.visible ? parent.spacing : 0)
                        height: 40
                        color: "#073642"
                        radius: 6

                        TextInput {
                            id: input
                            anchors.fill: parent
                            anchors.margins: 8
                            color: "#839496"
                            font.family: "monospace"
                            font.pixelSize: 14
                            clip: true
                            focus: true

                            onAccepted: {
                                if (text.trim().length > 0) {
                                    chatModel.addUserMessage(text);
                                    text = "";
                                }
                            }
                        }
                    }

                    // Pinned surfaces
                    Row {
                        id: pinnedRow
                        spacing: 4
                        visible: workspaceModel.pinnedCount > 0
                        anchors.verticalCenter: parent.verticalCenter

                        Repeater {
                            id: pinnedRepeater
                            model: workspaceModel

                            Item {
                                visible: model.pinned
                                width: visible ? pinnedSurfaceItem.implicitWidth || 200 : 0
                                height: visible ? 40 : 0

                                onWidthChanged: sendPinnedSize()
                                onHeightChanged: sendPinnedSize()
                                onVisibleChanged: sendPinnedSize()

                                function sendPinnedSize() {
                                    if (visible && width > 0 && height > 0) {
                                        compositor.sendPinnedConfigure(model.surfaceId, Math.round(width), Math.round(height));
                                    }
                                }

                                ShellSurfaceItem {
                                    id: pinnedSurfaceItem
                                    shellSurface: model.xdgSurface
                                    autoCreatePopupItems: false
                                    anchors.fill: parent
                                }
                            }
                        }
                    }
                }
            }
        }
    } // WaylandMouseTracker

    Connections {
        target: compositor
        function onHotkeyFocusChat() {
            input.forceActiveFocus();
        }
        function onHotkeyActivateWorkspace(index) {
            // index is 0-based unpinned position; map to model row
            var row = workspaceModel.nthUnpinnedIndex(index);
            if (row >= 0)
                workspaceModel.activateWorkspace(row);
        }
        function onHotkeyCycleWorkspace(direction) {
            var current = workspaceModel.activeIndex;
            if (current < 0) return;
            var next = workspaceModel.nextUnpinnedIndex(current, direction);
            if (next >= 0)
                workspaceModel.activateWorkspace(next);
        }
    }
}
