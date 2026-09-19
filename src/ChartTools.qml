import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
ColumnLayout {
    id: root
    property var sel: ({})
    readonly property var chart: sel.chart ?? ({})
    property string message: ""
    function update(key,value) { message=backend.setChartProperty(key,value) ? "" : qsTr("Those settings cannot be combined. Bounds must increase; bars and areas include zero, and log scales require positive values.") }
    spacing: Theme.s2
    ComboBox { objectName: "chartType"; Layout.fillWidth: true; model: backend.chartNames; currentIndex: root.chart.kind ?? 0; Accessible.name: qsTr("Chart type"); onActivated: root.update("kind",currentIndex) }
    Button { objectName: "editChartData"; Layout.fillWidth: true; text: qsTr("Edit chart data…"); onClicked: backend.editSelectedTable() }
    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; visible: (root.sel.chartIssues ?? []).length>0; text: (root.sel.chartIssues ?? []).join("\n"); color: Theme.accent }
    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; visible: (root.sel.chartWarnings ?? []).length>0; text: (root.sel.chartWarnings ?? []).join("\n"); color: Theme.textMuted }
    NumField { Layout.fillWidth: true; label: qsTr("Label size"); value: root.sel.fontSize ?? 18; onCommitted: value => backend.setSelectedProperty("fontSize",value) }
    TextField { objectName: "chartTitle"; Layout.fillWidth: true; text: root.chart.title ?? ""; placeholderText: qsTr("Chart title"); Accessible.name: qsTr("Chart title"); onEditingFinished: root.update("title",text) }
    TextField { Layout.fillWidth: true; text: root.chart.xTitle ?? ""; placeholderText: qsTr("Category / X axis title"); Accessible.name: placeholderText; onEditingFinished: root.update("xTitle",text) }
    TextField { Layout.fillWidth: true; text: root.chart.yTitle ?? ""; placeholderText: qsTr("Value / Y axis title"); Accessible.name: placeholderText; onEditingFinished: root.update("yTitle",text) }
    RowLayout {
        CheckBox { text: qsTr("Legend"); checked: root.chart.legend ?? true; onClicked: root.update("legend",checked) }
        CheckBox { text: qsTr("Grid"); checked: root.chart.grid ?? true; onClicked: root.update("grid",checked) }
    }
    CheckBox { objectName: "chartLabels"; text: qsTr("Value labels"); checked: root.chart.labels ?? false; onClicked: root.update("labels",checked) }
    SectionLabel { text: qsTr("DATA AND NUMBERS") }
    ComboBox { Layout.fillWidth: true; model: [qsTr("English (UK) · 1,234.56"),qsTr("German · 1.234,56"),qsTr("French · 1 234,56")]; currentIndex: ["en_GB","de_DE","fr_FR"].indexOf(root.chart.locale ?? "en_GB"); Accessible.name: qsTr("Chart data locale"); onActivated: root.update("locale",["en_GB","de_DE","fr_FR"][currentIndex]) }
    ComboBox { Layout.fillWidth: true; model: [qsTr("General"),qsTr("Fixed decimals"),qsTr("Percent"),qsTr("Currency")]; currentIndex: root.chart.numberFormat ?? 0; Accessible.name: qsTr("Chart number format"); onActivated: root.update("numberFormat",currentIndex) }
    NumField { Layout.fillWidth: true; visible: (root.chart.numberFormat ?? 0)>0; label: qsTr("Decimals"); value: root.chart.decimals ?? 1; onCommitted: value => root.update("decimals",value) }
    TextField { Layout.fillWidth: true; visible: root.chart.numberFormat===3; text: root.chart.currency ?? "£"; Accessible.name: qsTr("Currency prefix"); onEditingFinished: root.update("currency",text) }
    SectionLabel { text: qsTr("SCALES"); visible: root.chart.kind!==7 && root.chart.kind!==8  }
    ColumnLayout {
        Layout.fillWidth: true; visible: root.chart.kind!==7 && root.chart.kind!==8
        CheckBox { text: qsTr("Include zero"); enabled: root.chart.kind===4 || root.chart.kind===9; checked: root.chart.includeZero ?? true; onClicked: root.update("includeZero",checked) }
        CheckBox { objectName: "chartLogarithmic"; text: qsTr("Logarithmic values"); visible: root.chart.kind===4 || root.chart.kind===9; checked: root.chart.logarithmic ?? false; onClicked: root.update("logarithmic",checked) }
        CheckBox { text: qsTr("Custom value bounds"); checked: root.chart.manualY ?? false; onClicked: root.update("manualY",checked) }
        NumField { Layout.fillWidth: true; label: qsTr("Minimum"); value: root.chart.minimumY ?? 0; step: .1; onCommitted: value => root.update("minimumY",value) }
        NumField { Layout.fillWidth: true; label: qsTr("Maximum"); value: root.chart.maximumY ?? 100; step: .1; onCommitted: value => root.update("maximumY",value) }
        CheckBox { visible: root.chart.kind===9; text: qsTr("Custom X bounds"); checked: root.chart.manualX ?? false; onClicked: root.update("manualX",checked) }
        NumField { Layout.fillWidth: true; visible: root.chart.kind===9; label: qsTr("X minimum"); value: root.chart.minimumX ?? 0; step: .1; onCommitted: value => root.update("minimumX",value) }
        NumField { Layout.fillWidth: true; visible: root.chart.kind===9; label: qsTr("X maximum"); value: root.chart.maximumX ?? 100; step: .1; onCommitted: value => root.update("maximumX",value) }
    }
    SectionLabel { text: qsTr("COLOURS") }
    ComboBox { objectName: "chartPalette"; Layout.fillWidth: true; model: [qsTr("Theme · restrained"),qsTr("Muted categories"),qsTr("High contrast · patterned")]; currentIndex: root.chart.palette ?? 0; Accessible.name: qsTr("Chart palette"); onActivated: root.update("palette",currentIndex) }
    ComboBox { id: series; Layout.fillWidth: true; model: root.sel.chartSeries ?? []; textRole: "name"; valueRole: "id"; Accessible.name: qsTr("Chart series or category") }
    TextField { id: seriesColor; Layout.fillWidth: true; text: (root.sel.chartSeries ?? [])[series.currentIndex]?.color ?? ""; Accessible.name: qsTr("Series colour"); onEditingFinished: backend.setChartSeriesColor(series.currentValue,text) }
    Button { text: qsTr("Set deck palette slot %1").arg(series.currentIndex+1); enabled: series.currentIndex>=0 && series.currentIndex<8; onClicked: backend.setChartThemeColor(series.currentIndex,seriesColor.text) }
    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsCaption; text: qsTr("Deck palette colours update every chart using Theme. Series colour overrides affect this chart only.") }
    Button { text: qsTr("Use palette colour"); enabled: series.currentIndex>=0; onClicked: backend.setChartSeriesColor(series.currentValue,"") }
    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; visible: root.message.length>0; text: root.message; color: Theme.accent }
}
