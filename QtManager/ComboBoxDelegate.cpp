#include "ComboBoxDelegate.h"
 

 
ComboBoxDelegate::ComboBoxDelegate(QObject *parent)
:QItemDelegate(parent)
{
}
 
 
QWidget *ComboBoxDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex &/* index */) const
{
    QComboBox* editor = new QComboBox(parent);
    for(int i = 0; i < Items.size(); ++i)
    {
        editor->addItem(Items.at(i));
    }
    return editor;
}
 
void ComboBoxDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    comboBox->setCurrentText(index.model()->data(index, Qt::EditRole).toString());
}
 
void ComboBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    model->setData(index, comboBox->currentText(), Qt::EditRole);
}
 
void ComboBoxDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/* index */) const
{
  editor->setGeometry(option.rect);
}
/*
void ComboBoxDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  QStyleOptionViewItem myOption = option;
  QString text = Items[index.row()].c_str();
 
  myOption.text = text;
 
  QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &myOption, painter);
}*/
