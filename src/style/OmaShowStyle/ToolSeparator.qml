import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.ToolSeparator {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: vertical ? Theme.s2 : Theme.s1
    leftPadding: vertical ? Theme.s1 : Theme.s2
    rightPadding: leftPadding
    contentItem: Rectangle {
        implicitWidth: control.vertical ? Theme.hairline : Theme.s5
        implicitHeight: control.vertical ? Theme.s5 : Theme.hairline
        color: Theme.border
    }
}
