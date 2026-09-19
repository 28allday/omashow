import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

// A hairline at rest that thickens on hover so it can be grabbed.
T.ScrollBar {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: 2
    minimumSize: 0.06
    visible: control.policy !== T.ScrollBar.AlwaysOff
    contentItem: Rectangle {
        implicitWidth: control.interactive ? (control.hovered || control.pressed ? 8 : 5) : 2
        implicitHeight: implicitWidth
        radius: width / 2
        color: control.pressed ? Theme.textSecondary : control.hovered ? Theme.textMuted : Theme.borderStrong
        opacity: control.policy === T.ScrollBar.AlwaysOn || control.active && control.size < 1.0 ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: Theme.dNormal; easing.type: Theme.easing } }
    }
}
