import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.ProgressBar {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    contentItem: Item {
        implicitWidth: Theme.wInspector / 2
        implicitHeight: 4
        Rectangle {
            width: control.indeterminate ? parent.width / 3 : control.position * parent.width
            height: parent.height
            radius: 2
            color: Theme.accent
        }
    }
    background: Rectangle {
        implicitWidth: Theme.wInspector / 2
        implicitHeight: 4
        y: (control.height - height) / 2
        height: 4
        radius: 2
        color: Theme.borderStrong
    }
}
