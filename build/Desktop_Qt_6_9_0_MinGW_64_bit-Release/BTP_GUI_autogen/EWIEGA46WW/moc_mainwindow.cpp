/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../mainwindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "onSelectDEM",
        "",
        "onStartSimulation",
        "onPauseSimulation",
        "onStopSimulation",
        "onSimulationStep",
        "onOutletMethodChanged",
        "index",
        "updateUIState",
        "onManualOutletCellsToggled",
        "checked",
        "onSaveResults",
        "onClearOutlets",
        "onSelectOutlet",
        "onSimDisplayClicked",
        "pos",
        "updateVisualization",
        "resetVisualizationView",
        "onToggleGrid",
        "onToggleRulers",
        "onGridIntervalChanged",
        "value",
        "onTimeVaryingRainfallToggled",
        "onAddRainfallRow",
        "onRemoveRainfallRow",
        "updateRainfallSchedule",
        "onResolutionChanged",
        "newResolution"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onSelectDEM'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onStartSimulation'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPauseSimulation'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onStopSimulation'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSimulationStep'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onOutletMethodChanged'
        QtMocHelpers::SlotData<void(int)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 8 },
        }}),
        // Slot 'updateUIState'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onManualOutletCellsToggled'
        QtMocHelpers::SlotData<void(bool)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 11 },
        }}),
        // Slot 'onSaveResults'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearOutlets'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSelectOutlet'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSimDisplayClicked'
        QtMocHelpers::SlotData<void(QPoint)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QPoint, 16 },
        }}),
        // Slot 'updateVisualization'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'resetVisualizationView'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onToggleGrid'
        QtMocHelpers::SlotData<void(bool)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 11 },
        }}),
        // Slot 'onToggleRulers'
        QtMocHelpers::SlotData<void(bool)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 11 },
        }}),
        // Slot 'onGridIntervalChanged'
        QtMocHelpers::SlotData<void(int)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 22 },
        }}),
        // Slot 'onTimeVaryingRainfallToggled'
        QtMocHelpers::SlotData<void(bool)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 11 },
        }}),
        // Slot 'onAddRainfallRow'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRemoveRainfallRow'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateRainfallSchedule'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onResolutionChanged'
        QtMocHelpers::SlotData<void(double)>(27, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Double, 28 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onSelectDEM(); break;
        case 1: _t->onStartSimulation(); break;
        case 2: _t->onPauseSimulation(); break;
        case 3: _t->onStopSimulation(); break;
        case 4: _t->onSimulationStep(); break;
        case 5: _t->onOutletMethodChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->updateUIState(); break;
        case 7: _t->onManualOutletCellsToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->onSaveResults(); break;
        case 9: _t->onClearOutlets(); break;
        case 10: _t->onSelectOutlet(); break;
        case 11: _t->onSimDisplayClicked((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 12: _t->updateVisualization(); break;
        case 13: _t->resetVisualizationView(); break;
        case 14: _t->onToggleGrid((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->onToggleRulers((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->onGridIntervalChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 17: _t->onTimeVaryingRainfallToggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 18: _t->onAddRainfallRow(); break;
        case 19: _t->onRemoveRainfallRow(); break;
        case 20: _t->updateRainfallSchedule(); break;
        case 21: _t->onResolutionChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 22)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 22;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 22)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 22;
    }
    return _id;
}
QT_WARNING_POP
