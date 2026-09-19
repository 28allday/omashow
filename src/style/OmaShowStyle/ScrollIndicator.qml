import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.ScrollIndicator {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: 2
    contentItem: Rectangle {
        implicitWidth: 3
        implicitHeight: 3
        radius: 1.5
        color: Theme.borderStrong
        visible: control.size < 1.0
        opacity: control.active ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.dNormal } }
    }
}
