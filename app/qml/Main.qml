import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import OctPlayer 1.0

ApplicationWindow
{
    width: 1000
    height: 650
    visible: true
    title: "OctPlayer"
    property string durationText: "00:00"

    function formatTime(ms)
    {
        var value = Number(ms);
        if (!isFinite(value) || value < 0)
            return "00:00";

        if (value === 0)
            return "00:00";

        var totalSeconds = Math.floor(value / 1000);
        var minutes = Math.floor(totalSeconds / 60);
        var seconds = totalSeconds % 60;

        return (minutes < 10 ? "0" : "") + minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
    }

    Connections
    {
        target: player

        function onDurationChanged()
        {
            console.log("Duration changed: " + player.durationMs);
            durationText = formatTime(player.durationMs)
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 8

        VideoOutput
        {
            id: video
            Layout.fillWidth: true
            Layout.fillHeight: true
            Component.onCompleted: video.setMailbox(player.videoMailbox())
        }

        ColumnLayout
        {
            Layout.fillWidth: true
            spacing: 6

            RowLayout
            {
                Layout.fillWidth: true
                spacing: 8

                Slider
                {
                    id: seekSlider
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(player.durationMs, 0)
                    property real pendingSeekMs: 0
                    property bool seekChanged: false
                    enabled: player.seekable && player.durationMs > 0 &&
                             (player.state === 2 || player.state === 3 || player.state === 4)

                    onValueChanged:
                    {
                        if (pressed)
                        {
                            pendingSeekMs = value
                            seekChanged = true
                        }
                    }

                    onPressedChanged:
                    {
                        if (pressed)
                        {
                            pendingSeekMs = value
                            seekChanged = false
                        }
                        else if (seekChanged && enabled)
                        {
                            player.seek(Math.round(pendingSeekMs))
                            seekChanged = false
                        }
                    }
                }

                Binding
                {
                    target: seekSlider
                    property: "value"
                    value: player.positionMs
                    when: !seekSlider.pressed
                }

                Label
                {
                    id: timeLabel
                    text: formatTime(player.positionMs) + " / " + durationText
                    color: "#E3F2FD"
                    font.pixelSize: 12
                    Layout.minimumWidth: 140
                }
            }

            RowLayout
            {
                Layout.fillWidth: true
                spacing: 8

                TextField
                {
                    id: source
                    Layout.fillWidth: true
                    // text: "D:\\SW\\shareDir\\Focal20.04\\VideoDir\\aDir\\dasMusic.mp3"
                    text: "D:\\SW\\shareDir\\Focal20.04\\VideoDir\\vDir\\276982.mp4"
                    placeholderText: "Media file path or URL"

                }

                Button
                {
                    text: "Open"
                    onClicked: player.open(source.text)
                    flat: false
                    highlighted: false

                    background: Rectangle
                    {
                        implicitWidth: 120
                        implicitHeight: 40
                        color: parent.down ? "#1565C0"
                                        : (parent.hovered ? "#42A5F5" : "#1976D2")
                        radius: 8
                        border.color: "#0D47A1"
                        border.width: 1
                    }
                }

                Button
                {
                    text: player.state === 4 ? "Resume" : (player.state === 3 ? "Playing" : "Play")
                    enabled: player.state === 2 || player.state === 4
                    onClicked: player.state === 4 ? player.resume() : player.play()
                    flat: false
                    highlighted: false

                    background: Rectangle
                    {
                        implicitWidth: 120
                        implicitHeight: 40
                        color: parent.down ? "#1565C0"
                                        : (parent.hovered ? "#42A5F5" : "#1976D2")
                        radius: 8
                        border.color: "#0D47A1"
                        border.width: 1
                    }
                }

                Button
                {
                    text: "Pause"
                    enabled: player.state === 3
                    onClicked: player.pause()
                    flat: false
                    highlighted: false

                    background: Rectangle
                    {
                        implicitWidth: 120
                        implicitHeight: 40
                        color: parent.down ? "#1565C0"
                                        : (parent.hovered ? "#42A5F5" : "#1976D2")
                        radius: 8
                        border.color: "#0D47A1"
                        border.width: 1
                    }
                }

                Button
                {
                    text: "Stop"
                    enabled: player.state !== 0
                    onClicked: player.stop()
                    flat: false
                    highlighted: false

                    background: Rectangle
                    {
                        implicitWidth: 120
                        implicitHeight: 40
                        color: parent.down ? "#1565C0"
                                        : (parent.hovered ? "#42A5F5" : "#1976D2")
                        radius: 8
                        border.color: "#0D47A1"
                        border.width: 1
                    }
                }
            }
        }
    }
}
