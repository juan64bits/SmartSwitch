#ifndef COMBOBOXDELEGATE_H
#define COMBOBOXDELEGATE_H
 
#include <QComboBox>
#include <QWidget>
#include <QModelIndex>
#include <QApplication>
#include <QString>
#include <QStringList>
#include <QItemDelegate>
#include <iostream>
 
class QModelIndex;
class QWidget;
class QVariant;
 
class ComboBoxDelegate : public QItemDelegate
{
Q_OBJECT
public:
  ComboBoxDelegate(QObject *parent = 0);
 
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
  void setEditorData(QWidget *editor, const QModelIndex &index) const;
  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
  void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;
  void setItems(QStringList items)
  {Items.clear(); Items << items;}

private:
  QStringList Items;
 
};
#endif
