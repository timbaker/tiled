#ifndef UNDOREDOBUTTONS_H
#define UNDOREDOBUTTONS_H

#include <QObject>

class QToolButton;
class QUndoStack;

class UndoRedoButtons : public QObject
{
    Q_OBJECT
public:
    UndoRedoButtons(QUndoStack *undoStack, QObject *parent = 0);

    void resetIndex();

    QToolButton *undoButton() const
    { return mUndo; }

    QToolButton *redoButton() const
    { return mRedo; }

public slots:
    void updateActions();
    void textChanged();
    void retranslateUi();

private:
    QUndoStack *mUndoStack;
    QToolButton *mUndo;
    QToolButton *mRedo;
    int mUndoIndex;
};
#endif // UNDOREDOBUTTONS_H
