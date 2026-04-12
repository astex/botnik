import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtWayland.Compositor 1.15

Rectangle {
    id: root
    color: "#002b36"

    readonly property bool hasActiveWorkspace: workspaceModel.activeIndex >= 0

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Left: workspace area with tab bar (gets majority of screen)
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.hasActiveWorkspace
            spacing: 0

            // Tab bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                color: "#073642"
                radius: 4

                ListView {
                    id: tabBar
                    anchors.fill: parent
                    anchors.margins: 2
                    orientation: ListView.Horizontal
                    model: workspaceModel
                    spacing: 4
                    clip: true

                    delegate: Rectangle {
                        width: tabLabel.implicitWidth + 24
                        height: tabBar.height
                        color: index === workspaceModel.activeIndex ? "#2aa198" : "#002b36"
                        radius: 3

                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: model.title
                            color: index === workspaceModel.activeIndex ? "#002b36" : "#839496"
                            font.family: "monospace"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: workspaceModel.activateWorkspace(index)
                        }
                    }
                }
            }

            // Workspace surface area
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

                    ShellSurfaceItem {
                        anchors.fill: parent
                        shellSurface: model.xdgSurface
                        autoCreatePopupItems: false
                        visible: index === workspaceModel.activeIndex
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

            Rectangle {
                Layout.fillWidth: true
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
        }
    }

    Connections {
        target: compositor
        function onHotkeyFocusChat() {
            input.forceActiveFocus();
        }
        function onHotkeyActivateWorkspace(index) {
            workspaceModel.activateWorkspace(index);
        }
    }
}
