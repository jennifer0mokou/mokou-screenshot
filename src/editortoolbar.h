#pragma once
#include "document.h"
#include <QFrame>
#include <QPointer>
class QLabel;
class QToolButton;
class QVBoxLayout;
class EditorToolbar : public QFrame
{
    Q_OBJECT
public:
    explicit EditorToolbar(Document* document, QWidget* overlay);
    ~EditorToolbar() override;
    void setStatus(const QString& text);
    void setBusy(bool busy);
    void positionOptions();
    void closeOptions();
signals:
    void toolRequested(Document::Tool tool);
    void ocrRequested();
    void translateRequested();
    void describeRequested();
    void settingsRequested();
    void collageRequested();
    void recordRequested(bool systemAudio, bool microphone);
    void accepted();
    void canceled();
    void cancelRecognitionRequested();
    void preferencesChanged();
    void copyTextRequested();
    void layoutChanged();

protected:
    bool eventFilter(QObject*, QEvent*) override;

private:
    Document* doc;
    QWidget* surface;
    QPointer<QFrame> options;
    QVBoxLayout* optionsLayout;
    QWidget* optionAnchor = nullptr;
    QLabel* status;
    QToolButton* cancelTask;
    bool busy = false;
    void openOptions(QWidget* anchor, const QString& title);
    void textOptions(QWidget* anchor);
    void pickerOptions(QWidget* anchor);
    void brushOptions(QWidget* anchor);
    void recordingOptions(QWidget* anchor);
    void moreOptions(QWidget* anchor);
    void palette(QVBoxLayout* layout, bool text);
    void styleChanged();
};
