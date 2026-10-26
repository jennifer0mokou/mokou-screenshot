#pragma once
#include "document.h"
#include <QPointer>
#include <QWidget>
class EditorToolbar;
class QScrollArea;
class QLabel;
class AnnotationInput;
class QScreen;
class TextOverlay;
class ReadingPanel;
class Overlay : public QWidget
{
    Q_OBJECT
public:
    Overlay(Document* document, QScreen* screen, bool demo = false);
    void setStatus(const QString& text);
    void setRecognitionBusy(bool busy);
signals:
    void accepted();
    void canceled();
    void ocrRequested();
    void translateRequested();
    void describeRequested();
    void settingsRequested();
    void collageRequested();
    void recordRequested(bool systemAudio, bool microphone);
    void cancelRecognitionRequested();
    void preferencesChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    Document* doc;
    QPointF offset;
    EditorToolbar* bar = nullptr;
    QScrollArea* toolbarViewport = nullptr;
    QLabel* hint = nullptr;
    TextOverlay* textLayer = nullptr;
    int readingOpacity = -1;
    ReadingPanel* reading = nullptr;
    void layoutReadingPanel();
    void syncResults();
    QPointer<AnnotationInput> textInput;
    QPointF anchor, pointer;
    QRectF initialSelection;
    Mark preview;
    QBrush mosaicPreview;
    bool dragging = false, drawing = false, moving = false;
    int edges = 0;
    bool recognitionBusy = false;
    void buildToolbar();
    void layoutToolbar();
    int hitEdges(const QPointF& point) const;
    QPointF docPoint(const QPointF& local) const;
    void finishText();
    void chooseTool(Document::Tool tool);
    void updateCursor();
    void acceptCapture();
};
