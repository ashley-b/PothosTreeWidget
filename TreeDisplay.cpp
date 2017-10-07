// Copyright (c) 2017-2017 Ashley Brighthope
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <QBoxLayout>
#include <QLabel>
#include <QTreeView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <Poco/Logger.h>

/***********************************************************************
 * |PothosDoc Tree Display
 *
 * The tree display widget can display object and packets in a tree structure,
 * useful for debugging.
 * The display value can be set through setValue() slots.
 *
 * |category /Widgets
 * |keywords tree display
 *
 * |param title The name of the value displayed by this widget
 * |default "Object as Tree"
 * |widget StringEntry()
 *
 * |mode graphWidget
 * |factory /widgets/tree_display()
 * |setter setTitle(title)
 **********************************************************************/


static int PothosObjectId = qRegisterMetaType< Pothos::Object >("Pothos::Object"); // Register type for QT slot


class TreeDisplay : public QWidget, public Pothos::Block
{
    Q_OBJECT
public:

    static Block *make(void)
    {
        return new TreeDisplay();
    }

    static Poco::Logger& logger()
    {
        static Poco::Logger &_logger = Poco::Logger::get("TreeDisplay");
        return _logger;
    }

    TreeDisplay(void):
        _layout(new QVBoxLayout(this)),
        _label(new QLabel(this)),
        _treeView(new QTreeView(this)),
        _standardItemModel(new QStandardItemModel())
    {
        // Setup GUI
        _layout->setContentsMargins(QMargins());
        _layout->addWidget(_label);
        _layout->addWidget(_treeView);

        QStringList headerStr;
        headerStr.append("Key/Index");
        headerStr.append("Value");
        _standardItemModel->setHorizontalHeaderLabels(headerStr);

        _treeView->setModel(_standardItemModel.get());

        // Setup Pothos
        this->registerCall(this, POTHOS_FCN_TUPLE(TreeDisplay, widget));
        this->registerCall(this, POTHOS_FCN_TUPLE(TreeDisplay, setTitle));
        this->registerCall(this, POTHOS_FCN_TUPLE(TreeDisplay, setValue));
    }

    QWidget *widget(void)
    {
        return this;
    }

    void setTitle(const QString &title)
    {
        QMetaObject::invokeMethod(_label, "setText", Qt::QueuedConnection, Q_ARG(QString, title));
    }

    QString objectToString(const Pothos::Object &object)
    {
        return QString("(%1) %2")
            .arg(QString::fromStdString(object.getTypeString()))
            .arg(QString::fromStdString(object.toString()));
    }

    QStandardItem* createAndAppendRow(QStandardItem *parent, const QString &key, const Pothos::Object &object, const QString &value)
    {
        QList< QStandardItem* > standardItemList;
        auto keyItem = new QStandardItem(key);
        // Make item readonly
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        standardItemList.append(keyItem);

        auto dataItem = new QStandardItem(value);
        // Make item readonly
        dataItem->setFlags(dataItem->flags() & ~Qt::ItemIsEditable);
        // Set the tool tip to show the data type for this item
        dataItem->setToolTip(QString::fromStdString(object.getTypeString()));
        standardItemList.append(dataItem);

        parent->appendRow(standardItemList);
        return standardItemList[0];
    }

    QStandardItem* createAndAppendRow(QStandardItem *parent, const QString &key, const Pothos::Object &object)
    {
        return createAndAppendRow(parent, key, object, objectToString(object));
    }

    void walkObject(QStandardItem *parent, const QString &key, const Pothos::Object &object)
    {
        if (object.canConvert(typeid(Pothos::ObjectVector)))
        {
            auto item = createAndAppendRow(parent, key, object, QString::fromStdString(object.getTypeString()));
            try {
                const auto objectVector = object.convert< Pothos::ObjectVector >();
                for (size_t i = 0; i < objectVector.size(); ++i)
                {
                    walkObject(item, QString::number(i), objectVector[i]);
                }
            }
            catch (const std::exception &e) {
                poco_error(logger(), "walkObject Pothos::ObjectVector exception: " + std::string(e.what()));
            }
            return;
        }

        if (object.canConvert(typeid(Pothos::ObjectMap)))
        {
            auto item = createAndAppendRow(parent, key, object, QString::fromStdString(object.getTypeString()));
            try {
                const auto objectMap = object.convert< Pothos::ObjectMap >();
                for (const auto &pair : objectMap)
                {
                    walkObject(item, objectToString(pair.first), pair.second);
                }
            }
            catch (const std::exception &e) {
                poco_error(logger(), "walkObject Pothos::ObjectMap exception: " + std::string(e.what()));
            }
            return;
        }

        if (object.canConvert(typeid(Pothos::Packet)))
        {
            auto item = createAndAppendRow(parent, key, object, QString::fromStdString(object.getTypeString()));
            try {
                const auto packet = object.convert< Pothos::Packet >();

                Pothos::ObjectMap packetObject;

                // Convert Pothos::Packet to a Pothos::Object to simplify displaying in tree

                {
                    Pothos::ObjectKwargs payloadObject;
                    payloadObject["dtype"] = Pothos::Object(packet.payload.dtype.toMarkup());
                    payloadObject["length"] = Pothos::Object(packet.payload.length);
                    packetObject[Pothos::Object("payload")] = Pothos::Object::make(payloadObject);
                }

                packetObject[Pothos::Object("metadata")] = Pothos::Object::make(packet.metadata);

                {
                    Pothos::ObjectVector labelsObject;
                    for (const auto &label : packet.labels)
                    {
                        Pothos::ObjectKwargs labelObject;
                        labelObject["data"] = label.data;
                        labelObject["id"] = Pothos::Object(label.id);
                        labelObject["index"] = Pothos::Object(label.index);
                        labelObject["width"] = Pothos::Object(label.width);

                        labelsObject.push_back(Pothos::Object(labelObject));
                    }
                    packetObject[Pothos::Object("labels")] = Pothos::Object::make(labelsObject);
                }

                for (const auto &pair : packetObject)
                {
                    walkObject(item, objectToString(pair.first), pair.second);
                }
            }
            catch (const std::exception &e) {
                poco_error(logger(), "walkObject Pothos::Packet exception: " + std::string(e.what()));
            }
            return;
        }

        // For all other data types use built in convert
        createAndAppendRow(parent, key, object);
        return;
    }

    void setValue(const Pothos::Object &object)
    {
        QMetaObject::invokeMethod(this, "setTreeValue", Qt::QueuedConnection, Q_ARG(Pothos::Object, object));
    }

public slots:

    void setTreeValue(Pothos::Object object)
    {
        _standardItemModel->removeRows(0, _standardItemModel->rowCount(QModelIndex()), QModelIndex());
        walkObject(_standardItemModel->invisibleRootItem(), QString(), object);

        _treeView->expandAll();
    }

    QVariant saveState(void) const
    {
        return _treeView->header()->saveState();
    }

    void restoreState(const QVariant &state)
    {
        _treeView->header()->restoreState(state.toByteArray());
    }

private:
    QVBoxLayout *_layout;
    QLabel *_label;
    QTreeView *_treeView;
    std::unique_ptr< QStandardItemModel > _standardItemModel;
};

static Pothos::BlockRegistry registerTextDisplay(
    "/widgets/tree_display", &TreeDisplay::make);

#include "TreeDisplay.moc"
