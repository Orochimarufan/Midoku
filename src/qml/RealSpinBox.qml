import QtQuick 2.15
import QtQuick.Controls 2.15

SpinBox {
    property int decimals: 2
    property int factor: 10 ** decimals
    property real realValue: value / factor
    property real realFrom: 0
    property real realTo: 100
    property real realStep: 1

    from: realFrom * factor
    to: realTo * factor
    stepSize: realStep * factor

    onRealValueChanged: value = realValue * factor
    onValueModified: realValue = value / factor

    validator: DoubleValidator {
        bottom: realFrom
        top: realTo
        decimals: decimals
    }

    textFromValue: function (value, locale) {
        return Number(value / factor).toLocaleString(locale, 'f', decimals)
    }

    valueFromText: function (text, locale) {
        return Number.fromLocaleString(locale, text) * factor
    }
}
