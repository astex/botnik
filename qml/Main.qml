import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#002b36"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

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

            // Also scroll when content of last message changes (streaming)
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
