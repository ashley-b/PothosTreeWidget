// Copyright (c) 2017-2017 Ashley Brighthope
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <QTreeView>
#include <QStandardItemModel>
#include <Poco/Logger.h>

/***********************************************************************
 * |PothosDoc Tree Display
 *
 * The tree display widget display's a object in a tree structure.
 * The display value can be set through setValue() slots.
 *
 * |category /Widgets
 * |keywords tree display
 *
 * |mode graphWidget
 * |factory /widgets/tree_display()
 **********************************************************************/

Poco::Logger& logger()
{
    static Poco::Logger &_logger = Poco::Logger::get("TreeDisplay");
    return _logger;
}

// Register type for QT slot
static int PothosObjectId = qRegisterMetaType< Pothos::Object >("Pothos::Object");


class TreeDisplay : public QTreeView, public Pothos::Block
{
    Q_OBJECT
public:

    static Block *make(void)
    {
        return new TreeDisplay();
    }

    TreeDisplay(void):
        _standardItemModel( new QStandardItemModel( ) )
    {
        // SEtup GUI
        QStringList headerStr;
        headerStr.append("Key/Index");
        headerStr.append("Value");
        _standardItemModel->setHorizontalHeaderLabels(headerStr);

        this->setModel(_standardItemModel.get());

        // Setup Pothos
        this->registerCall(this, POTHOS_FCN_TUPLE(TreeDisplay, widget));
        this->registerCall(this, POTHOS_FCN_TUPLE(TreeDisplay, setValue));
    }

    QWidget *widget(void)
    {
        return this;
    }

    QString objectToString(const Pothos::Object &object)
    {
        return QString("(%1) %2").
                arg(QString::fromStdString(object.getTypeString())).
                arg(QString::fromStdString(object.toString()));
    }

    QList< QStandardItem* > createRow(const Pothos::Object &object, const std::string &key, const std::string &value)
    {
        QList< QStandardItem* > standardItemList;
        auto keyItem = new QStandardItem(QString::fromStdString(key));
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        standardItemList.append(keyItem);

        auto dataItem = new QStandardItem( QString::fromStdString(value));
        dataItem->setFlags(dataItem->flags() & ~Qt::ItemIsEditable);
        dataItem->setToolTip(QString::fromStdString(object.getTypeString()));
        standardItemList.append(dataItem);

        return standardItemList;
    }

    QList< QStandardItem* > createRow(const Pothos::Object &object, const std::string &key)
    {
        return createRow(object, key, objectToString(object).toStdString());
    }

    void walkObject(const Pothos::Object &object, const std::string &key, QStandardItem *parent)
    {
        if (object.canConvert(typeid(Pothos::ObjectVector)))
        {
            auto item = createRow(object, key);
            parent->appendRow(item);
            try {
                const auto &objectVector = object.convert< Pothos::ObjectVector >();
                for (size_t i = 0; i < objectVector.size(); ++i)
                {
                    walkObject(objectVector[i], std::to_string(i), item[0]);
                }
            }
            catch (std::exception e) {
                poco_error(logger(), "walkObject Pothos::ObjectVector exception: " + std::string(e.what()));
            }
            return;
        }

        if (object.canConvert(typeid(Pothos::ObjectMap)))
        {
            auto item = createRow(object, key);
            parent->appendRow( item );
            try {
                const auto &objectMap = object.convert< Pothos::ObjectMap >();
                for (const auto &pair : objectMap)
                {
                    walkObject(pair.second, objectToString(pair.first).toStdString(), item[0]);
                }
            }
            catch (std::exception e) {
                poco_error(logger(), "walkObject Pothos::ObjectMap exception: " + std::string(e.what()));
            }
            return;
        }

        // For all other data types use built in convert
        parent->appendRow(createRow(object, key));
        return;
    }

    void setValue(const Pothos::Object &object)
    {
        QMetaObject::invokeMethod(this, "_setValue", Qt::QueuedConnection, Q_ARG(Pothos::Object, object));
    }

public slots:

    void _setValue(Pothos::Object object)
    {
        _standardItemModel->removeRows(0, _standardItemModel->rowCount(QModelIndex()), QModelIndex());
        walkObject(object, "", _standardItemModel->invisibleRootItem());

        this->expandAll();
    }

private:
    std::shared_ptr< QStandardItemModel > _standardItemModel;
};

static Pothos::BlockRegistry registerTextDisplay(
    "/widgets/tree_display", &TreeDisplay::make);

#include "TreeDisplay.moc"
