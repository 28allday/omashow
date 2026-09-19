import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.MenuSeparator {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.s1
    leftPadding: Theme.s2
    rightPadding: Theme.s2
    contentItem: Rectangle {
        implicitWidth: Theme.s5 * 6
        implicitHeight: Theme.hairline
        color: Theme.border
    }
}
