import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
ColumnLayout {
    id: root
    property var sel: backend.selection
    Layout.fillWidth: true
    SectionLabel { text: qsTr("FOCAL POINT (%)"); visible: root.sel.imageMode===1  }
    RowLayout {
        visible: root.sel.imageMode===1
        NumField { objectName: "imageFocalX"; Layout.fillWidth: true; label: qsTr("X"); value: (root.sel.imageFocalX??.5)*100; onCommitted: v=>backend.setSelectedProperty("imageFocalX",v/100) }
        NumField { objectName: "imageFocalY"; Layout.fillWidth: true; label: qsTr("Y"); value: (root.sel.imageFocalY??.5)*100; onCommitted: v=>backend.setSelectedProperty("imageFocalY",v/100) }
    }
    SectionLabel { text: qsTr("MASK") }
    ComboBox { objectName: "imageMask"; Layout.fillWidth: true; model: [qsTr("Rectangle"),qsTr("Ellipse"),qsTr("Rounded rectangle"),qsTr("Hexagon"),qsTr("Heart")]; currentIndex: root.sel.imageMask??0; onActivated: backend.setSelectedProperty("imageMask",currentIndex) }
    NumField { Layout.fillWidth: true; visible: root.sel.imageMask===2; label: qsTr("Radius"); value: root.sel.cornerRadius || Math.min(root.sel.w,root.sel.h)*.1; onCommitted: v=>backend.setSelectedProperty("cornerRadius",v) }
    ColumnLayout {
        visible: root.sel.imageFormat!=="svg"; Layout.fillWidth: true
        SectionLabel { text: qsTr("COLOUR ADJUSTMENTS") }
        NumField { objectName: "imageBrightness"; Layout.fillWidth: true; label: qsTr("Brightness %"); value: (root.sel.imageBrightness??0)*100; onCommitted: v=>backend.setSelectedProperty("imageBrightness",v/100) }
        NumField { objectName: "imageContrast"; Layout.fillWidth: true; label: qsTr("Contrast %"); value: (root.sel.imageContrast??1)*100; onCommitted: v=>backend.setSelectedProperty("imageContrast",v/100) }
        NumField { objectName: "imageSaturation"; Layout.fillWidth: true; label: qsTr("Saturation %"); value: (root.sel.imageSaturation??1)*100; onCommitted: v=>backend.setSelectedProperty("imageSaturation",v/100) }
        TextField { Layout.fillWidth: true; text: root.sel.imageTint??"#000000"; Accessible.name: qsTr("Tint colour"); onEditingFinished: backend.setSelectedProperty("imageTint",text) }
        NumField { Layout.fillWidth: true; label: qsTr("Tint %"); value: (root.sel.imageTintAmount??0)*100; onCommitted: v=>backend.setSelectedProperty("imageTintAmount",v/100) }
        Button { objectName: "resetImageAdjustments"; text: qsTr("Reset colour adjustments"); onClicked: backend.resetImageAdjustments() }
    }
    Label { Layout.fillWidth: true; wrapMode: Text.Wrap; visible: root.sel.imageFormat==="svg" && ((root.sel.imageBrightness??0)!==0 || (root.sel.imageContrast??1)!==1 || (root.sel.imageSaturation??1)!==1 || (root.sel.imageTintAmount??0)!==0); text: qsTr("This SVG retains its vector colours. Saved raster colour adjustments are inactive."); color: Theme.textMuted }
}
