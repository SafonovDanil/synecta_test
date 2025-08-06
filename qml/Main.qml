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

    // Dragndrop
    DropArea {
        anchors.fill: parent
        
        onEntered: function(drag) {
            if (drag.hasUrls) {
                var hasValidFile = false;
                for (var i = 0; i < drag.urls.length; i++) {
                    var url = drag.urls[i].toString();
                    if (url.toLowerCase().endsWith('.s1p') || url.toLowerCase().endsWith('.S1P')) {
                        hasValidFile = true;
                        break;
                    }
                }
                
                if (hasValidFile && !backend.isLoading) {
                    drag.accept(Qt.CopyAction);
                    dropOverlay.visible = true;
                } else {
                    drag.accepted = false;
                }
            } else {
                drag.accepted = false;
            }
        }
        
        onExited: {
            dropOverlay.visible = false;
        }
        
        onDropped: function(drop) {
            dropOverlay.visible = false;
            
            if (drop.hasUrls && !backend.isLoading) {
                for (var i = 0; i < drop.urls.length; i++) {
                    var url = drop.urls[i].toString();
                    if (url.toLowerCase().endsWith('.s1p') || url.toLowerCase().endsWith('.S1P')) {
                        backend.loadFile(drop.urls[i]);
                        drop.accept(Qt.CopyAction);
                        return;
                    }
                }
            }
            drop.accepted = false;
        }
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
                text: "📁 Load Touchstone File"
                enabled: !backend.isLoading
                onClicked: fileDialog.open()
                
                ToolTip.visible: hovered
                ToolTip.text: "Load .s1p file or drag & drop it anywhere"
                ToolTip.delay: 500
            }

            Button {
                text: "Clear"
                enabled: backend.hasData && !backend.isLoading
                onClicked: backend.clearData()
            }

            Button {
                text: "Reset Zoom"
                enabled: backend.hasData && backend.isZoomed && !backend.isLoading
                onClicked: backend.resetZoom()
            }

            BusyIndicator {
                running: backend.isLoading
                visible: backend.isLoading
                implicitWidth: 24
                implicitHeight: 24
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: backend.isLoading ? "Loading..." : backend.hasData ? "Data loaded successfully" : "No data"
                color: backend.isLoading ? "orange" : backend.hasData ? "green" : "gray"
            }

            Text {
                text: backend.isZoomed ? "Zoomed" : ""
                color: "blue"
                visible: backend.isZoomed
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
                
                loadingText: "Loading graph..."
                emptyText: "Load a Touchstone file to display S11 graph\n\nDrag & drop .s1p files here or click 'Load Touchstone File'"
                
                Component.onCompleted: {
                    backend.setGraphWidget(graphWidget);
                }

                property bool isSelecting: false
                property point selectionStart
                property point selectionEnd

                MouseArea {
                    anchors.fill: parent
                    enabled: graphWidget.hasData && !graphWidget.isLoading

                    onPressed: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            graphWidget.isSelecting = true;
                            graphWidget.selectionStart = Qt.point(mouse.x, mouse.y);
                            graphWidget.selectionEnd = Qt.point(mouse.x, mouse.y);
                        }
                    }

                    onPositionChanged: function(mouse) {
                        if (graphWidget.isSelecting) {
                            graphWidget.selectionEnd = Qt.point(mouse.x, mouse.y);
                        }
                    }

                    onReleased: function(mouse) {
                        if (graphWidget.isSelecting && mouse.button === Qt.LeftButton) {
                            graphWidget.isSelecting = false;

                            var x1 = graphWidget.selectionStart.x;
                            var y1 = graphWidget.selectionStart.y;
                            var x2 = graphWidget.selectionEnd.x;
                            var y2 = graphWidget.selectionEnd.y;

                            // Зум если выделена достаточно большая область
                            if (Math.abs(x2 - x1) > 20 && Math.abs(y2 - y1) > 20) {
                                backend.zoomToPixelRegion(x1, y1, x2, y2, graphWidget.width, graphWidget.height);
                            }
                        }
                    }
                }

                Rectangle {
                    visible: graphWidget.isSelecting
                    x: Math.min(graphWidget.selectionStart.x, graphWidget.selectionEnd.x)
                    y: Math.min(graphWidget.selectionStart.y, graphWidget.selectionEnd.y)
                    width: Math.abs(graphWidget.selectionEnd.x - graphWidget.selectionStart.x)
                    height: Math.abs(graphWidget.selectionEnd.y - graphWidget.selectionStart.y)
                    color: "transparent"
                    border.color: "red"
                    border.width: 2
                }

                // Zoom
                Text {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.margins: 10
                    text: "Drag to zoom"
                    color: "#999"
                    font.pointSize: 10
                    visible: graphWidget.hasData && !graphWidget.isLoading
                }
            }
        }

        // Status bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#f5f5f5"
            border.color: "#ddd"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10

                Text {
                    text: backend.isLoading ? "Processing file..." : backend.hasData ? "Graph ready" : "Ready"
                    color: "#666"
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: backend.hasData ? "Data loaded" : ""
                    color: "#666"
                    visible: backend.hasData
                }
            }
        }
    }

    // Dragndrop overlay
    Rectangle {
        id: dropOverlay
        anchors.fill: parent
        color: "transparent"
        visible: false
        z: 1000
        
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        
        Rectangle {
            anchors.fill: parent
            color: "#4CAF50"
            opacity: 0.1
            border.color: "#4CAF50"
            border.width: 3
            radius: 8
            
            SequentialAnimation on opacity {
                running: dropOverlay.visible
                loops: Animation.Infinite
                NumberAnimation { to: 0.05; duration: 1000 }
                NumberAnimation { to: 0.15; duration: 1000 }
            }
        }
        
        Rectangle {
            anchors.centerIn: parent
            width: 300
            height: 160
            color: "white"
            border.color: "#4CAF50"
            border.width: 2
            radius: 8
            opacity: 0.95
            
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 10
                
                Text {
                    text: "📁"
                    font.pointSize: 32
                    Layout.alignment: Qt.AlignHCenter
                    
                    SequentialAnimation on scale {
                        running: dropOverlay.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.1; duration: 500; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 500; easing.type: Easing.InOutQuad }
                    }
                }
                
                Text {
                    text: "Drop Touchstone file here"
                    font.pointSize: 16
                    font.bold: true
                    color: "#4CAF50"
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Text {
                    text: "Supported formats: .s1p, .S1P"
                    font.pointSize: 12
                    color: "#666"
                    Layout.alignment: Qt.AlignHCenter
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
