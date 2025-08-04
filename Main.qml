import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import com.s11analyzer 1.0

ApplicationWindow {
    id: window
    width: 1000
    height: 700
    visible: true
    title: "Touchstone S11 Viewer"

    Backend {
        id: backend
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // Control panel
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: "Load Touchstone File"
                enabled: !backend.isLoading
                onClicked: fileDialog.open()
            }

            // Loading indicator
            BusyIndicator {
                running: backend.isLoading
                visible: backend.isLoading
                implicitWidth: 24
                implicitHeight: 24
            }

            Item {
                Layout.fillWidth: true
            }
        }

        // Error message
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: errorText.contentHeight + 20
            color: "#ffebee"
            border.color: "#f44336"
            border.width: 1
            radius: 4
            visible: backend.errorMessage !== ""

            Text {
                id: errorText
                anchors.centerIn: parent
                text: backend.errorMessage
                color: "#d32f2f"
                wrapMode: Text.WordWrap
                width: parent.width - 20
            }
        }

        // Graph area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "white"
            border.color: "#ccc"
            border.width: 1

            GraphWidget {
                id: graphWidget
                anchors.fill: parent
                anchors.margins: 1

                
                Component.onCompleted: {
                    backend.setGraphWidget(graphWidget);
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Select Touchstone File"
        nameFilters: ["Touchstone files (*.s1p *.S1P)", "All files (*)"]
        onAccepted: {
            if (fileDialog.currentFile !== undefined) {
                backend.loadFile(fileDialog.currentFile);
            }
        }
    }
}
