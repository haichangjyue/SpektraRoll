// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Qt 6 desktop shell (D009). The window deliberately does not reimplement the
// colour or texture pipeline: preview and render both drive the same
// `spektra-roll-cli` binary that CI exercises, so the UI cannot drift from the
// command line. Qt is dynamically linked and deployed with windeployqt.

#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

#include <cmath>
#include <functional>
#include <utility>

#ifdef _WIN32
#include <windows.h>

int main(int argc, char** argv);

// WIN32 subsystem entry point; MinGW links -mwindows but the body is main().
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
#endif

namespace {

QString exeSuffix() {
#ifdef _WIN32
    return QStringLiteral(".exe");
#else
    return QString();
#endif
}

// Palette ported from SpektraLab (windows_UI/qml/Theme.qml, GPL-3.0): grey
// ground around the canvas, dark card rails, warm amber accent, compact type.
const char* kStyleSheet = R"QSS(
QWidget { background-color: #2d2d2c; color: #faf8f4; font-family: "Segoe UI"; font-size: 12px; }
QWidget#toolbar { background: #2d2d2c; border-bottom: 1px solid #4a4a48; }
QWidget#centre { background: #5f5f5f; }
QWidget#infoBar { background: #2d2d2c; border-top: 1px solid #3f3f3d; }
QLabel { background: transparent; }
QLabel#appTitle { font-size: 14px; font-weight: 700; }
QLabel#railTitle { font-size: 14px; font-weight: 700; }
QLabel#muted { color: #b7b6b3; font-size: 10px; }
QLabel#fieldLabel { color: #d6d4cf; font-size: 11px; }
QLabel#note { color: #b7b6b3; font-size: 10px; }
QLabel#materialValue { background: #484846; border-radius: 3px; padding: 4px 6px; color: #faf8f4; }
QLabel#badge { color: #e1a95f; border: 1px solid #e1a95f; border-radius: 2px; padding: 1px 6px; font-size: 9px; letter-spacing: 1px; }
QFrame#sectionLine { color: #4a4a48; background: #4a4a48; max-height: 1px; }
QToolButton#sectionHeader { text-align: left; border: none; background: transparent; padding: 6px 10px; font-weight: 600; font-size: 12px; color: #faf8f4; }
QToolButton#sectionHeader:hover { background: #343432; }
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: #484846; border: 1px solid #565653; border-radius: 3px; padding: 2px 5px; min-height: 20px; selection-background-color: #e1a95f; selection-color: #252523; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid #e1a95f; }
QDoubleSpinBox#valueSpin { background: transparent; border: none; padding: 0; min-height: 16px; color: #b7b6b3; }
QComboBox QAbstractItemView { background: #2d2d2c; border: 1px solid #565653; selection-background-color: #e1a95f; selection-color: #252523; outline: 0; }
QPushButton { background: #484846; border: none; border-radius: 3px; padding: 4px 10px; min-height: 22px; }
QPushButton:hover { background: #585752; }
QPushButton:pressed { background: #686762; }
QPushButton:disabled { color: #777773; background: #3c3c3a; }
QPushButton[accent="true"] { background: #645039; border: 1px solid #e1a95f; }
QPushButton[accent="true"]:hover { background: #745c42; }
QPushButton:checked { background: #645039; border: 1px solid #e1a95f; }
QSlider::groove:horizontal { height: 2px; background: #5a5a57; border-radius: 1px; }
QSlider::sub-page:horizontal { background: #e1a95f; border-radius: 1px; }
QSlider::handle:horizontal { background: #faf8f4; width: 10px; height: 10px; margin: -4px 0; border-radius: 5px; }
QTabWidget::pane { border: none; border-top: 1px solid #4a4a48; }
QTabBar::tab { background: transparent; color: #b7b6b3; padding: 8px 12px; border: none; }
QTabBar::tab:selected { color: #faf8f4; border-bottom: 2px solid #e1a95f; }
QTabBar::tab:hover { color: #faf8f4; }
QHeaderView::section { background: #2d2d2c; color: #b7b6b3; border: none; border-bottom: 1px solid #4a4a48; padding: 5px; }
QTableWidget { background: #252523; alternate-background-color: #2b2b29; gridline-color: #3a3a38; border: none; }
QTableWidget::item { padding: 3px; }
QTableWidget::item:selected { background: #4a4a46; color: #faf8f4; }
QProgressBar { background: #1e1e1c; border: none; border-radius: 2px; text-align: center; color: #faf8f4; font-size: 10px; }
QProgressBar::chunk { background: #e1a95f; border-radius: 2px; }
QPlainTextEdit { background: #1e1e1c; border: 1px solid #3a3a38; color: #d8d6d1; selection-background-color: #e1a95f; }
QScrollArea { border: none; }
QScrollBar:vertical { background: #2d2d2c; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #5a5a57; min-height: 24px; border-radius: 5px; }
QScrollBar::handle:vertical:hover { background: #6a6a66; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: #2d2d2c; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #5a5a57; min-width: 24px; border-radius: 5px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QSplitter::handle { background: #4a4a48; width: 1px; }
QCheckBox { spacing: 6px; }
QToolTip { background: #1e1e1c; color: #faf8f4; border: 1px solid #565653; padding: 3px; }
)QSS";

QString number(double value, int decimals = 3) {
    return QString::number(value, 'f', decimals);
}

QString formatClock(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    const int minutes = static_cast<int>(seconds) / 60;
    const double rest = seconds - minutes * 60.0;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(rest, 5, 'f', 2, QLatin1Char('0'));
}

QString formatDuration(qint64 milliseconds) {
    if (milliseconds < 0) milliseconds = 0;
    const qint64 total = milliseconds / 1000;
    const qint64 hours = total / 3600;
    const qint64 minutes = (total % 3600) / 60;
    const qint64 seconds = total % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

struct ClipInfo {
    int width = 0;
    int height = 0;
    double fps = 0.0;
    double duration = 0.0;
    long frames = 0;
    QString timecode;
    // YUV -> RGB matrix from the stream tag ("bt2020nc", "bt709", ...). Empty
    // when the container does not say (D061).
    QString colorMatrix;
    bool valid() const { return width > 0 && height > 0 && fps > 0.0; }
};

// A queued render. The argument list is frozen at enqueue time, and the LUT is
// copied so a later parameter change cannot alter jobs already in the queue.
struct RenderJob {
    QString input;
    QString output;
    QString lutSnapshot;
    QStringList args;
    QString status = QStringLiteral("等待");
    int done = 0;
    int total = 0;
    qint64 startedMs = 0;
    qint64 finishedMs = 0;
};

class PreviewView : public QWidget {
public:
    explicit PreviewView(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(480, 300);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
        setCursor(Qt::ArrowCursor);
    }

    void setImage(const QImage& image) {
        image_ = image;
        computeHistogram();
        update();
        notifyZoom();
    }

    void fitToWindow() {
        fit_ = true;
        offset_ = QPointF();
        setCursor(Qt::ArrowCursor);
        update();
        notifyZoom();
    }

    void setZoom(double zoom) {
        if (image_.isNull()) return;
        fit_ = false;
        zoom_ = qBound(0.05, zoom, 8.0);
        offset_ = QPointF();
        clampOffset();
        setCursor(Qt::OpenHandCursor);
        update();
        notifyZoom();
    }

    void setZoomCallback(std::function<void(double, bool)> callback) {
        callback_ = std::move(callback);
        notifyZoom();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0x5f, 0x5f, 0x5f));
        const QRectF area = imageArea();
        if (image_.isNull()) {
            painter.setPen(QColor(0xd0, 0xcd, 0xc6));
            painter.drawText(area, Qt::AlignCenter, QStringLiteral("导入视频，选择胶片与质感"));
            return;
        }

        const QRectF target = imageRect();
        painter.setRenderHint(QPainter::SmoothPixmapTransform, currentScale() < 1.0);
        painter.drawImage(target, image_);
        painter.setPen(QColor(0, 0, 0, 70));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(target);
        drawHistogram(painter);
    }

    void wheelEvent(QWheelEvent* event) override {
        if (image_.isNull()) return;
        const QRectF old = imageRect();
        if (old.width() <= 0.0 || old.height() <= 0.0) return;
        const QPointF cursor = event->position();
        const double relX = qBound(0.0, (cursor.x() - old.left()) / old.width(), 1.0);
        const double relY = qBound(0.0, (cursor.y() - old.top()) / old.height(), 1.0);
        const double factor = event->angleDelta().y() >= 0 ? 1.15 : 1.0 / 1.15;
        const double next = qBound(0.05, currentScale() * factor, 8.0);

        fit_ = false;
        zoom_ = next;
        const QSizeF size(image_.width() * zoom_, image_.height() * zoom_);
        const QPointF topLeft(cursor.x() - relX * size.width(),
                              cursor.y() - relY * size.height());
        offset_ = topLeft + QPointF(size.width() / 2.0, size.height() / 2.0) -
                  imageArea().center();
        clampOffset();
        update();
        setCursor(Qt::OpenHandCursor);
        notifyZoom();
        event->accept();
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && !image_.isNull() && !fit_) {
            panning_ = true;
            lastPos_ = event->position();
            setCursor(Qt::ClosedHandCursor);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!panning_) return;
        offset_ += event->position() - lastPos_;
        lastPos_ = event->position();
        clampOffset();
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;
        panning_ = false;
        setCursor(fit_ ? Qt::ArrowCursor : Qt::OpenHandCursor);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton || image_.isNull()) return;
        if (fit_) {
            setZoom(1.0);
        } else {
            fitToWindow();
        }
    }

private:
    QRectF imageArea() const { return QRectF(rect()).adjusted(18, 18, -18, -18); }

    double fitScale() const {
        if (image_.isNull()) return 1.0;
        const QRectF area = imageArea();
        return qMin(area.width() / image_.width(), area.height() / image_.height());
    }

    double currentScale() const { return fit_ ? fitScale() : zoom_; }

    QRectF imageRect() const {
        if (image_.isNull()) return QRectF();
        const QSizeF size(image_.width() * currentScale(), image_.height() * currentScale());
        const QPointF center = imageArea().center() + (fit_ ? QPointF() : offset_);
        return QRectF(center.x() - size.width() / 2.0, center.y() - size.height() / 2.0,
                      size.width(), size.height());
    }

    void clampOffset() {
        if (image_.isNull()) {
            offset_ = QPointF();
            return;
        }
        const QRectF area = imageArea();
        const QSizeF size(image_.width() * zoom_, image_.height() * zoom_);
        const double maxX = qMax(0.0, (size.width() - area.width()) / 2.0);
        const double maxY = qMax(0.0, (size.height() - area.height()) / 2.0);
        offset_.setX(qBound(-maxX, offset_.x(), maxX));
        offset_.setY(qBound(-maxY, offset_.y(), maxY));
    }

    void notifyZoom() {
        if (!callback_) return;
        if (image_.isNull()) {
            callback_(0.0, true);
            return;
        }
        callback_(fit_ ? fitScale() * 100.0 : zoom_ * 100.0, fit_);
    }

    void computeHistogram() {
        histR_ = QVector<int>(256, 0);
        histG_ = QVector<int>(256, 0);
        histB_ = QVector<int>(256, 0);
        histMax_ = 1;
        if (image_.isNull()) return;
        const QImage sample = image_.convertToFormat(QImage::Format_RGB32);
        const int step = qMax(1, sample.width() / 700);
        for (int y = 0; y < sample.height(); y += step) {
            const QRgb* line = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
            for (int x = 0; x < sample.width(); x += step) {
                const QRgb pixel = line[x];
                ++histR_[qRed(pixel)];
                ++histG_[qGreen(pixel)];
                ++histB_[qBlue(pixel)];
            }
        }
        for (int i = 0; i < 256; ++i) {
            histMax_ = std::max(histMax_, histR_[i]);
            histMax_ = std::max(histMax_, histG_[i]);
            histMax_ = std::max(histMax_, histB_[i]);
        }
    }

    void drawHistogram(QPainter& painter) const {
        const QRectF box(width() - 18 - 190, 18, 190, 72);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(24, 24, 22, 195));
        painter.drawRect(box);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QColor(90, 90, 86));
        painter.drawRect(box);

        painter.setPen(QColor(255, 255, 255, 22));
        for (int i = 1; i < 4; ++i) {
            const double x = box.left() + box.width() * i / 4.0;
            painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
        }
        painter.drawLine(QPointF(box.left(), box.center().y()),
                         QPointF(box.right(), box.center().y()));

        const auto channel = [&](const QVector<int>& histogram, const QColor& colour) {
            QPainterPath path;
            path.moveTo(box.left(), box.bottom());
            for (int i = 0; i < 256; ++i) {
                const double x = box.left() + box.width() * i / 255.0;
                const double y = box.bottom() - box.height() * histogram[i] /
                                                     static_cast<double>(histMax_);
                path.lineTo(x, y);
            }
            path.lineTo(box.right(), box.bottom());
            path.closeSubpath();
            painter.setPen(QPen(colour, 1.0));
            painter.setBrush(QColor(colour.red(), colour.green(), colour.blue(), 80));
            painter.drawPath(path);
        };
        channel(histR_, QColor(230, 95, 85));
        channel(histG_, QColor(110, 190, 115));
        channel(histB_, QColor(95, 145, 215));

        painter.setPen(QColor(205, 202, 195));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 7));
        painter.drawText(box.adjusted(6, 4, -6, -4), Qt::AlignTop | Qt::AlignLeft,
                         QStringLiteral("RGB"));
    }

    QImage image_;
    bool fit_ = true;
    double zoom_ = 1.0;
    QPointF offset_;
    bool panning_ = false;
    QPointF lastPos_;
    QVector<int> histR_;
    QVector<int> histG_;
    QVector<int> histB_;
    int histMax_ = 1;
    std::function<void(double, bool)> callback_;
};

// Collapsible rail section, ported from SpektraLab's Section.qml (thin rule,
// bold title, disclosure triangle, compact 10-12 px labels).
class CollapsibleSection : public QWidget {
public:
    explicit CollapsibleSection(const QString& title, bool expanded = true,
                                QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* line = new QFrame;
        line->setObjectName(QStringLiteral("sectionLine"));
        line->setFrameShape(QFrame::HLine);
        outer->addWidget(line);

        header_ = new QToolButton;
        header_->setObjectName(QStringLiteral("sectionHeader"));
        header_->setText(title);
        header_->setCheckable(true);
        header_->setChecked(expanded);
        header_->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        header_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        outer->addWidget(header_);

        content_ = new QWidget;
        contentLayout_ = new QVBoxLayout(content_);
        contentLayout_->setContentsMargins(14, 4, 14, 12);
        contentLayout_->setSpacing(8);
        content_->setVisible(expanded);
        outer->addWidget(content_);

        connect(header_, &QToolButton::toggled, this, [this](bool on) {
            content_->setVisible(on);
            header_->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        });
    }

    void addWidget(QWidget* widget) { contentLayout_->addWidget(widget); }
    void addLayout(QLayout* layout) { contentLayout_->addLayout(layout); }

private:
    QToolButton* header_ = nullptr;
    QWidget* content_ = nullptr;
    QVBoxLayout* contentLayout_ = nullptr;
};

// Label + numeric readout on one line, thin slider underneath: the adjustment
// row SpektraLab uses for every parameter.
class SliderRow : public QWidget {
public:
    SliderRow(const QString& label, double minimum, double maximum, double step, int decimals,
              double value, const QString& suffix = QString(), QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(1);

        auto* header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        auto* name = new QLabel(label);
        spin_ = new QDoubleSpinBox;
        spin_->setObjectName(QStringLiteral("valueSpin"));
        spin_->setRange(minimum, maximum);
        spin_->setSingleStep(step);
        spin_->setDecimals(decimals);
        spin_->setSuffix(suffix);
        spin_->setValue(value);
        spin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin_->setAlignment(Qt::AlignRight);
        spin_->setFixedWidth(66);
        header->addWidget(name);
        header->addStretch(1);
        header->addWidget(spin_);

        slider_ = new QSlider(Qt::Horizontal);
        slider_->setRange(0, static_cast<int>(std::lround((maximum - minimum) * 1000.0)));
        slider_->setSingleStep(std::max(1, static_cast<int>(std::lround(step * 1000.0))));
        slider_->setValue(
            static_cast<int>(std::lround((value - minimum) * 1000.0)));
        slider_->setFixedHeight(16);

        connect(slider_, &QSlider::valueChanged, this, [this, minimum](int position) {
            spin_->setValue(minimum + position / 1000.0);
        });
        connect(spin_, &QDoubleSpinBox::valueChanged, this, [this, minimum](double v) {
            const QSignalBlocker blocker(slider_);
            slider_->setValue(static_cast<int>(std::lround((v - minimum) * 1000.0)));
        });

        outer->addLayout(header);
        outer->addWidget(slider_);
    }

    QDoubleSpinBox* spin() const { return spin_; }
    QSlider* slider() const { return slider_; }

private:
    QDoubleSpinBox* spin_ = nullptr;
    QSlider* slider_ = nullptr;
};

class MainWindow : public QWidget {
public:
    MainWindow() {
        repoRoot_ = findRepoRoot();
        cliPath_ = findCli();
        buildUi();
        loadSettings();
        setMinimumSize(980, 620);
        setAcceptDrops(true);
        probeInput();
        loadQueue();
        setWindowTitle(QStringLiteral("SpektraRoll — 胶片与印片模拟"));
    }

    // Scripted end-to-end check (`--preview-selftest`): runs the same preview
    // path as the button and returns the status line the UI would show.
    QString previewSelfTest() {
        previewFrame();
        return status_ != nullptr ? status_->text() : QString();
    }

    // `--red-selftest` (D071): load the known Nikon ZR sample through the same
    // probe/bake/preview path as the buttons. Requires build-red + SDK.
    QString redSelfTest() {
        const QString sample = QStringLiteral("X:/agent/footage/ZR_RED_Color_Science_Sunflower.R3D");
        if (!QFileInfo::exists(sample)) return QStringLiteral("error: sample R3D not found");
        inputEdit_->setText(sample);
        probeInput();
        if (!redAvailable_) {
            return QStringLiteral("error: RED plugin not available: ") + status_->text();
        }
        if (!spectralEngine()) {
            lutEdit_->setText(repoRoot_ + QStringLiteral("/out/ui-red-selftest.cube"));
            if (!bakeLut(false)) {
                return QStringLiteral("error: LUT bake failed: ") + status_->text();
            }
        }
        previewFrame();
        const QString result = status_ != nullptr ? status_->text() : QString();
        if (result.startsWith(QStringLiteral("预览"))) return QStringLiteral("ok: ") + result;
        return QStringLiteral("error: ") + result;
    }

    // `--red-render-selftest` (D071): run two R3D frames through the actual
    // Render button path (args, ensureLut, QProcess) and wait for it.
    QString redRenderSelfTest() {
        const QString sample = QStringLiteral("X:/agent/footage/ZR_RED_Color_Science_Sunflower.R3D");
        if (!QFileInfo::exists(sample)) return QStringLiteral("error: sample R3D not found");
        inputEdit_->setText(sample);
        probeInput();
        if (!redAvailable_) {
            return QStringLiteral("error: RED plugin not available: ") + status_->text();
        }
        if (!spectralEngine()) {
            lutEdit_->setText(repoRoot_ + QStringLiteral("/out/ui-red-render.cube"));
            if (!bakeLut(false)) {
                return QStringLiteral("error: LUT bake failed: ") + status_->text();
            }
        }
        frames_->setValue(2);
        start_->setValue(0.0);
        outputEdit_->setText(repoRoot_ + QStringLiteral("/out/ui-red-render.mov"));
        suppressCompletionDialog_ = true;
        startRender();
        if (!rendering_) {
            return QStringLiteral("error: render did not start: ") + status_->text();
        }
        QEventLoop loop;
        QTimer poll;
        connect(&poll, &QTimer::timeout, &loop, [this, &loop] {
            if (!rendering_) loop.quit();
        });
        poll.start(200);
        QTimer::singleShot(180000, &loop, &QEventLoop::quit);
        loop.exec();
        if (rendering_) return QStringLiteral("error: render timed out");
        const QString result = status_->text();
        if (result.startsWith(QStringLiteral("渲染完成"))) return QStringLiteral("ok: ") + result;
        return QStringLiteral("error: ") + result;
    }

    // `--stocks-selftest` (D065): the catalogue the UI builds from the profile
    // JSON, checked without clicking. Prints counts and the cinema entries.
    QString stocksSelfTest() {
        const auto has = [](QComboBox* combo, const QString& name) {
            for (int i = 0; i < combo->count(); ++i) {
                if (combo->itemText(i).contains(name)) return true;
            }
            return false;
        };
        const bool vision3 = has(filmCombo_, QStringLiteral("Vision3 500T"));
        const bool verita = has(filmCombo_, QStringLiteral("Verita"));
        const bool print2383 = has(paperCombo_, QStringLiteral("2383"));
        const bool print2393 = has(paperCombo_, QStringLiteral("2393"));
        const bool portra = has(filmCombo_, QStringLiteral("Portra 400")) &&
                            has(paperCombo_, QStringLiteral("Portra Endura"));
        const bool slide = has(filmCombo_, QStringLiteral("Ektachrome")) &&
                           has(filmCombo_, QStringLiteral("Velvia")) &&
                           has(filmCombo_, QStringLiteral("Provia")) &&
                           has(filmCombo_, QStringLiteral("Kodachrome"));
        const bool ok = vision3 && verita && print2383 && print2393 && portra && slide;
        return QStringLiteral(
                   "%1 films=%2 papers=%3 vision3=%4 verita=%5 print=%6/%7 portra=%8 slide=%9")
            .arg(ok ? QStringLiteral("ok") : QStringLiteral("error"))
            .arg(filmCombo_->count())
            .arg(paperCombo_->count())
            .arg(vision3)
            .arg(verita)
            .arg(print2383)
            .arg(print2393)
            .arg(portra)
            .arg(slide);
    }

    // `--ocio-selftest` (D063): read the bundled config through the CLI, pick
    // Apple Log and bake a LUT through the same path as the Bake button.
    QString ocioSelfTest() {
        if (ocioConfig_->text().trimmed().isEmpty()) ocioConfig_->setText(defaultOcioConfig());
        const QString config = ocioConfig_->text().trimmed();
        if (config.isEmpty() || !QFileInfo::exists(config)) {
            return QStringLiteral("error: no OCIO config (set OCIO or install data/ocio)");
        }
        {
            const QSignalBlocker blocker(ocioEnabled_);
            ocioEnabled_->setChecked(true);
        }
        updateOcioUi();
        refreshOcioSpaces(false);
        if (ocioSpace_->count() == 0) {
            return QStringLiteral("error: no colour spaces (") + ocioInfoLabel_->text() +
                   QLatin1Char(')');
        }
        ocioSpace_->setCurrentIndex(
            std::max(0, ocioSpace_->findData(QStringLiteral("S-Log3 S-Gamut3.Cine"))));
        // A LUT bake is the point of this check; the spectral engines do not
        // need one (and would make this a no-op).
        {
            const QSignalBlocker blocker(engine_);
            const int lutIndex = engine_->findData(QStringLiteral("lut"));
            if (lutIndex >= 0) engine_->setCurrentIndex(lutIndex);
        }
        // Fixed look, independent of whatever the last session saved, so the
        // demo can reproduce this exact table with one `lut bake` command.
        const auto selectData = [](QComboBox* combo, const QString& data) {
            const QSignalBlocker blocker(combo);
            const int index = combo->findData(data);
            if (index >= 0) combo->setCurrentIndex(index);
        };
        const auto selectStock = [](QComboBox* combo, const QString& fileName) {
            for (int i = 0; i < combo->count(); ++i) {
                if (QFileInfo(combo->itemData(i).toString()).fileName() == fileName) {
                    const QSignalBlocker blocker(combo);
                    combo->setCurrentIndex(i);
                    return;
                }
            }
        };
        selectData(trc_, QStringLiteral("srgb"));
        selectData(scan_, QStringLiteral("negative"));
        selectStock(filmCombo_, QStringLiteral("kodak_portra_400.json"));
        selectStock(paperCombo_, QStringLiteral("kodak_portra_endura.json"));
        exposure_->setValue(0.7);
        contrast_->setValue(1.15);
        couplers_->setChecked(true);
        gamut_->setChecked(true);
        updateEngineUi();
        lutEdit_->setText(repoRoot_ + QStringLiteral("/out/ui-ocio-selftest.cube"));
        if (!bakeLut(false)) {
            QString tail = log_->toPlainText().trimmed();
            const qsizetype newline = tail.lastIndexOf(QLatin1Char('\n'));
            if (newline >= 0) tail = tail.mid(newline + 1);
            return QStringLiteral("error: LUT bake failed: ") + tail;
        }
        return QStringLiteral("ok %1 spaces · %2 · %3 · %4 bytes")
            .arg(ocioSpace_->count())
            .arg(ocioSpace_->currentText(), lutEdit_->text())
            .arg(QFileInfo(lutEdit_->text()).size());
    }

    // Graceful exit for the long-lived preview process (also used by quit()).
    void shutdownPreview() { stopPreviewServer(); }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (!event->mimeData()->hasUrls()) return;
        for (const QUrl& url : event->mimeData()->urls()) {
            const QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
            if (suffix == QStringLiteral("mov") || suffix == QStringLiteral("mp4") ||
                suffix == QStringLiteral("mxf") || suffix == QStringLiteral("mkv")) {
                event->acceptProposedAction();
                return;
            }
        }
    }

    void dropEvent(QDropEvent* event) override {
        QStringList videos;
        for (const QUrl& url : event->mimeData()->urls()) {
            const QString path = url.toLocalFile();
            if (path.isEmpty()) continue;
            const QString suffix = QFileInfo(path).suffix().toLower();
            if (suffix == QStringLiteral("mov") || suffix == QStringLiteral("mp4") ||
                suffix == QStringLiteral("mxf") || suffix == QStringLiteral("mkv") ||
                suffix == QStringLiteral("r3d")) {
                videos << path;
            }
        }
        if (videos.isEmpty()) return;
        event->acceptProposedAction();

        if (videos.size() == 1) {
            inputEdit_->setText(QDir::toNativeSeparators(videos.first()));
            probeInput();
            return;
        }
        enqueueBatch(videos);
    }

    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        if (geometryChecked_) return;
        geometryChecked_ = true;
        QScreen* current = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
        if (current == nullptr) return;
        const QRect available = current->availableGeometry();
        if (!available.contains(frameGeometry())) {
            QRect frame = frameGeometry();
            frame.setWidth(qMin(frame.width(), available.width() - 80));
            frame.setHeight(qMin(frame.height(), available.height() - 80));
            frame.moveCenter(available.center());
            setGeometry(frame);
        }
    }

    void closeEvent(QCloseEvent* event) override {
        if (rendering_) {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("还在渲染"),
                QStringLiteral("渲染还没结束，确定退出吗？\n已完成的段会保留，下次可以勾选“断点续渲”。"));
            if (answer != QMessageBox::Yes) {
                event->ignore();
                return;
            }
            canceled_ = true;
            if (renderProcess_ != nullptr) {
                renderProcess_->kill();
                renderProcess_->waitForFinished(2000);
            }
        }
        if (queueRunning_) {
            const auto answer = QMessageBox::question(
                this, QStringLiteral("队列还在渲染"),
                QStringLiteral("队列还没跑完，确定退出吗？\n已完成的任务会保存，当前任务下次回到“等待”。"));
            if (answer != QMessageBox::Yes) {
                event->ignore();
                return;
            }
            queueRunning_ = false;
            queueCanceled_ = true;
            if (queueProcess_ != nullptr) {
                queueProcess_->kill();
                queueProcess_->waitForFinished(2000);
            }
        }
        saveSettings();
        saveQueue();
        stopPreviewServer();
        QWidget::closeEvent(event);
    }

private:
    // ---- environment -----------------------------------------------------

    QString findRepoRoot() const {
        QDir dir(QCoreApplication::applicationDirPath());
        for (int depth = 0; depth < 5; ++depth) {
            if (QFileInfo::exists(dir.filePath("data/profiles/kodak_portra_400.json"))) {
                return dir.absolutePath();
            }
            if (!dir.cdUp()) break;
        }
        return QDir::currentPath();
    }

    QString findCli() const {
        // D063: the UI drives the CLI, including its optional OpenColorIO.
        // scripts\build.cmd produces the full CLI with OCIO; build-ui uses the
        // Qt toolchain, which cannot link the OCIO static library (D062), so
        // prefer the full build when it exists. Set SPEKTRA_CLI to override.
        const QString fromEnv = qEnvironmentVariable("SPEKTRA_CLI");
        if (!fromEnv.isEmpty() && QFileInfo::exists(fromEnv)) return fromEnv;
        // A local RED plugin build is a superset of the normal CLI: it has the
        // same OCIO/film code plus the optional R3D loader. Prefer it when the
        // user built one, so `red-info` in the UI sees the plugin (D069).
        const QString red = repoRoot_ + "/build-red/spektra-roll-cli" + exeSuffix();
        if (QFileInfo::exists(red)) return red;
        const QString built = repoRoot_ + "/build/spektra-roll-cli" + exeSuffix();
        if (QFileInfo::exists(built)) return built;
        const QString local =
            QCoreApplication::applicationDirPath() + "/spektra-roll-cli" + exeSuffix();
        if (QFileInfo::exists(local)) return local;
        return local;
    }

    QString defaultFfmpeg() const {
        const QString fromEnv = qEnvironmentVariable("SPEKTRA_FFMPEG");
        if (!fromEnv.isEmpty()) return fromEnv;
        const QString known = QStringLiteral("J:/ffmpeg/bin/ffmpeg.exe");
        if (QFileInfo::exists(known)) return known;
        return QStringLiteral("ffmpeg");
    }

    QString defaultInput() const {
        const QString known = QStringLiteral("X:/agent/footage/C0113.MP4");
        return QFileInfo::exists(known) ? known : QString();
    }

    // Film stocks and second mediums the engine supports (D065/D067): D55/T
    // negative stocks printed on a paper or a cinema print film (2383/2393),
    // plus the reversal stocks that are scanned directly (D067).
    void populateStocks() {
        struct Stock {
            QString path;
            QString name;
            QString target;
            bool printFilm = false;
            bool positive = false;
        };
        QVector<Stock> films;
        QVector<Stock> papers;
        const QDir dir(repoRoot_ + QStringLiteral("/data/profiles"));
        const QFileInfoList files =
            dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        for (const QFileInfo& entry : files) {
            if (entry.fileName() == QStringLiteral("neutral_print_filters.json")) continue;
            QFile file(entry.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly)) continue;
            const QJsonObject info =
                QJsonDocument::fromJson(file.readAll())
                    .object()
                    .value(QStringLiteral("info"))
                    .toObject();
            const QString support = info.value(QStringLiteral("support")).toString();
            const QString stage = info.value(QStringLiteral("stage")).toString();
            const QString type = info.value(QStringLiteral("type")).toString();
            const QString illuminant =
                info.value(QStringLiteral("reference_illuminant")).toString();
            Stock stock{entry.absoluteFilePath(),
                        info.value(QStringLiteral("name")).toString(entry.completeBaseName()),
                        info.value(QStringLiteral("target_print")).toString(), false, false};
            const bool filming =
                support == QStringLiteral("film") && stage == QStringLiteral("filming") &&
                (type == QStringLiteral("negative") || type == QStringLiteral("positive")) &&
                (illuminant == QStringLiteral("D55") || illuminant == QStringLiteral("T"));
            const bool printFilm =
                support == QStringLiteral("film") && stage == QStringLiteral("printing");
            if (filming) {
                stock.positive = type == QStringLiteral("positive");
                films.append(stock);
            } else if (support == QStringLiteral("paper") || printFilm) {
                stock.printFilm = printFilm;
                papers.append(stock);
            }
        }

        filmCombo_->blockSignals(true);
        paperCombo_->blockSignals(true);
        filmCombo_->clear();
        paperCombo_->clear();
        for (const Stock& stock : films) {
            bool targetAvailable = stock.target.isEmpty();
            for (const Stock& paper : papers) {
                if (QFileInfo(paper.path).completeBaseName() == stock.target) {
                    targetAvailable = true;
                    break;
                }
            }
            if (!targetAvailable) continue;
            filmCombo_->addItem(
                stock.positive ? stock.name + QStringLiteral("（反转片）") : stock.name,
                stock.path);
            filmCombo_->setItemData(filmCombo_->count() - 1, stock.target, Qt::UserRole + 1);
            filmCombo_->setItemData(filmCombo_->count() - 1, stock.positive,
                                    Qt::UserRole + 2);
        }
        for (const Stock& stock : papers) {
            paperCombo_->addItem(
                stock.printFilm ? stock.name + QStringLiteral("（电影印片）") : stock.name,
                stock.path);
        }
        filmCombo_->blockSignals(false);
        paperCombo_->blockSignals(false);

        const auto selectFile = [](QComboBox* combo, const QString& fileName) {
            for (int i = 0; i < combo->count(); ++i) {
                if (QFileInfo(combo->itemData(i).toString()).fileName() == fileName) {
                    combo->setCurrentIndex(i);
                    return;
                }
            }
        };
        selectFile(filmCombo_, QStringLiteral("kodak_portra_400.json"));
        selectFile(paperCombo_, QStringLiteral("kodak_portra_endura.json"));
    }

    // YUV -> RGB matrix for ffmpeg, taken from the clip's own color_space tag
    // (D061). Guessing from the selected source was wrong as soon as several
    // cameras could be imported: some record their log inside a bt709
    // container and some inside bt2020. Untagged clips keep the pre-D061
    // guess (Apple Log bt2020, everything else bt709), so nothing regresses.
    QString decodeMatrix() const {
        const QString tag = clipInfo_.colorMatrix;
        if (tag.startsWith(QStringLiteral("bt2020"))) return QStringLiteral("bt2020");
        if (tag == QStringLiteral("bt709") || tag == QStringLiteral("smpte170m") ||
            tag == QStringLiteral("bt470bg") || tag == QStringLiteral("smpte240m")) {
            return tag;
        }
        const QString input = source_ != nullptr ? source_->currentData().toString() : QString();
        return input == QStringLiteral("apple-log") ? QStringLiteral("bt2020")
                                                    : QStringLiteral("bt709");
    }

    QString ffprobePath() const {
        const QString ffmpeg = ffmpegEdit_ != nullptr ? ffmpegEdit_->text().trimmed() : QString();
        if (ffmpeg.isEmpty()) return QStringLiteral("ffprobe");
        const QFileInfo info(ffmpeg);
        if (!info.isAbsolute()) return QStringLiteral("ffprobe");
        QString name = info.fileName();
        if (!name.contains(QStringLiteral("ffmpeg"), Qt::CaseInsensitive)) {
            return info.absolutePath() + QStringLiteral("/ffprobe");
        }
        name.replace(QStringLiteral("ffmpeg"), QStringLiteral("ffprobe"), Qt::CaseInsensitive);
        return info.absolutePath() + QLatin1Char('/') + name;
    }

    bool runCaptureSync(const QString& program, const QStringList& args, QString* output,
                        QString* errorText = nullptr) const {
        if (program.trimmed().isEmpty()) {
            *output = QString();
            if (errorText != nullptr) *errorText = QStringLiteral("程序路径为空");
            return false;
        }
        QProcess process;
        process.setWorkingDirectory(repoRoot_);
        process.start(program, args);
        if (!process.waitForStarted(10000)) {
            *output = QString();
            if (errorText != nullptr) *errorText = QStringLiteral("无法启动：") + program;
            return false;
        }
        process.waitForFinished(-1);
        *output = QString::fromLocal8Bit(process.readAllStandardOutput());
        if (errorText != nullptr) {
            *errorText = QString::fromLocal8Bit(process.readAllStandardError());
        }
        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

    ClipInfo probeClip(const QString& path) const {
        ClipInfo info;
        if (path.isEmpty() || !QFileInfo::exists(path)) return info;
        QString output;
        const QStringList args{
            QStringLiteral("-v"), QStringLiteral("error"),
            QStringLiteral("-select_streams"), QStringLiteral("v:0"),
            QStringLiteral("-show_streams"), QStringLiteral("-show_format"),
            QStringLiteral("-of"), QStringLiteral("json"), path};
        if (!runCaptureSync(ffprobePath(), args, &output)) {
            return info;
        }
        const QJsonObject root = QJsonDocument::fromJson(output.toUtf8()).object();
        const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
        if (streams.isEmpty()) return info;
        const QJsonObject stream = streams.first().toObject();
        info.width = stream.value(QStringLiteral("width")).toInt();
        info.height = stream.value(QStringLiteral("height")).toInt();
        const QJsonArray sideData = stream.value(QStringLiteral("side_data_list")).toArray();
        for (const QJsonValue& value : sideData) {
            const int rotation = value.toObject().value(QStringLiteral("rotation")).toInt();
            if (rotation == 90 || rotation == -90 || rotation == 270 || rotation == -270) {
                std::swap(info.width, info.height);
                break;
            }
        }
        const QStringList rate =
            stream.value(QStringLiteral("r_frame_rate")).toString().split(QLatin1Char('/'));
        if (rate.size() == 2 && rate[1].toDouble() > 0.0) {
            info.fps = rate[0].toDouble() / rate[1].toDouble();
        }
        const QJsonValue frameValue = stream.value(QStringLiteral("nb_frames"));
        info.frames = frameValue.isDouble() ? static_cast<long>(frameValue.toDouble())
                                            : frameValue.toString().toLong();
        info.timecode = stream.value(QStringLiteral("tags"))
                            .toObject()
                            .value(QStringLiteral("timecode"))
                            .toString();
        info.colorMatrix = stream.value(QStringLiteral("color_space")).toString();
        const QJsonObject format = root.value(QStringLiteral("format")).toObject();
        const QJsonValue durationValue = format.value(QStringLiteral("duration"));
        info.duration = durationValue.isDouble() ? durationValue.toDouble()
                                                  : durationValue.toString().toDouble();
        if (info.timecode.isEmpty()) {
            info.timecode = format.value(QStringLiteral("tags"))
                                .toObject()
                                .value(QStringLiteral("timecode"))
                                .toString();
        }
        if (info.duration <= 0.0 && info.fps > 0.0 && info.frames > 0) {
            info.duration = static_cast<double>(info.frames) / info.fps;
        }
        if (info.frames <= 0 && info.duration > 0.0 && info.fps > 0.0) {
            info.frames = static_cast<long>(std::llround(info.duration * info.fps));
        }
        return info;
    }

    // ---- ui --------------------------------------------------------------

    QWidget* pathRow(QLineEdit* edit, const QString& title, const QString& filter) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(edit, 1);
        auto* button = new QPushButton(QStringLiteral("…"));
        button->setFixedWidth(34);
        connect(button, &QPushButton::clicked, this, [this, edit, title, filter] {
            const QString file = QFileDialog::getOpenFileName(this, title, edit->text(), filter);
            if (!file.isEmpty()) edit->setText(QDir::toNativeSeparators(file));
        });
        layout->addWidget(button);
        return row;
    }

    QWidget* saveRow(QLineEdit* edit, const QString& title, const QString& filter) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(edit, 1);
        auto* button = new QPushButton(QStringLiteral("…"));
        button->setFixedWidth(34);
        connect(button, &QPushButton::clicked, this, [this, edit, title, filter] {
            const QString file = QFileDialog::getSaveFileName(this, title, edit->text(), filter);
            if (!file.isEmpty()) edit->setText(QDir::toNativeSeparators(file));
        });
        layout->addWidget(button);
        return row;
    }

    QDoubleSpinBox* amountSpin(double value) {
        auto* spin = new QDoubleSpinBox;
        spin->setRange(0.0, 1.0);
        spin->setSingleStep(0.05);
        spin->setDecimals(2);
        spin->setValue(value);
        return spin;
    }

    void buildUi() {
        inputEdit_ = new QLineEdit;
        ffmpegEdit_ = new QLineEdit;
        lutEdit_ = new QLineEdit;
        outputEdit_ = new QLineEdit;

        const auto labeledRow = [](const QString& text, QWidget* field) {
            auto* holder = new QWidget;
            auto* layout = new QVBoxLayout(holder);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(3);
            auto* label = new QLabel(text);
            label->setObjectName(QStringLiteral("fieldLabel"));
            layout->addWidget(label);
            layout->addWidget(field);
            return holder;
        };
        const auto noteLabel = [](const QString& text) {
            auto* label = new QLabel(text);
            label->setObjectName(QStringLiteral("note"));
            label->setWordWrap(true);
            return label;
        };

        // ---- top toolbar ---------------------------------------------------
        auto* toolbar = new QWidget;
        toolbar->setObjectName(QStringLiteral("toolbar"));
        toolbar->setFixedHeight(42);
        auto* toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setContentsMargins(12, 0, 12, 0);
        toolbarLayout->setSpacing(6);

        auto* titleBlock = new QWidget;
        auto* titleLayout = new QVBoxLayout(titleBlock);
        titleLayout->setContentsMargins(0, 0, 0, 0);
        titleLayout->setSpacing(0);
        auto* appTitle = new QLabel(QStringLiteral("SpektraRoll"));
        appTitle->setObjectName(QStringLiteral("appTitle"));
        auto* appSub = new QLabel(QStringLiteral("胶片与印片"));
        appSub->setObjectName(QStringLiteral("muted"));
        titleLayout->addWidget(appTitle);
        titleLayout->addWidget(appSub);
        toolbarLayout->addWidget(titleBlock);
        toolbarLayout->addSpacing(14);

        previewButton_ = new QPushButton(QStringLiteral("预览一帧"));
        previewButton_->setToolTip(QStringLiteral("Ctrl+P"));
        compareButton_ = new QPushButton(QStringLiteral("按住看原片"));
        compareButton_->setToolTip(QStringLiteral("按住显示未套胶片的原片，松开回到成片"));
        renderButton_ = new QPushButton(QStringLiteral("渲染整段"));
        renderButton_->setProperty("accent", true);
        renderButton_->setToolTip(QStringLiteral("Ctrl+R"));
        addQueueButton_ = new QPushButton(QStringLiteral("加入队列"));
        addQueueButton_->setToolTip(QStringLiteral("把当前参数冻结成一个队列任务"));
        cancelButton_ = new QPushButton(QStringLiteral("取消"));
        cancelButton_->setEnabled(false);
        auto* folderButton = new QPushButton(QStringLiteral("输出文件夹"));
        folderButton->setToolTip(QStringLiteral("在资源管理器里打开输出文件所在目录"));
        toolbarLayout->addWidget(previewButton_);
        toolbarLayout->addWidget(compareButton_);
        toolbarLayout->addWidget(renderButton_);
        toolbarLayout->addWidget(addQueueButton_);
        toolbarLayout->addWidget(cancelButton_);
        toolbarLayout->addWidget(folderButton);
        toolbarLayout->addStretch(1);

        clipNameLabel_ = new QLabel(QStringLiteral("未选择素材"));
        clipNameLabel_->setObjectName(QStringLiteral("muted"));
        clipNameLabel_->setMaximumWidth(280);
        toolbarLayout->addWidget(clipNameLabel_);
        zoomLabel_ = new QLabel(QStringLiteral("FIT"));
        zoomLabel_->setObjectName(QStringLiteral("muted"));
        zoomLabel_->setMinimumWidth(42);
        zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fitButton_ = new QPushButton(QStringLiteral("适合"));
        fitButton_->setCheckable(true);
        fitButton_->setAutoExclusive(true);
        fitButton_->setChecked(true);
        fitButton_->setToolTip(QStringLiteral("适合窗口 · Ctrl+0 / 双击画布"));
        zoom100Button_ = new QPushButton(QStringLiteral("100%"));
        zoom100Button_->setCheckable(true);
        zoom100Button_->setAutoExclusive(true);
        zoom100Button_->setToolTip(QStringLiteral("物理像素 100% · Ctrl+1 / 双击画布"));
        toolbarLayout->addWidget(zoomLabel_);
        toolbarLayout->addWidget(fitButton_);
        toolbarLayout->addWidget(zoom100Button_);

        // ---- left rail: materials, film, output ----------------------------
        auto* left = new QWidget;
        auto* leftLayout = new QVBoxLayout(left);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(0);
        auto* leftTitle = new QLabel(QStringLiteral("胶片与印相"));
        leftTitle->setObjectName(QStringLiteral("railTitle"));
        leftTitle->setFixedHeight(42);
        leftTitle->setContentsMargins(14, 0, 14, 0);
        leftLayout->addWidget(leftTitle);

        auto* materialSection = new CollapsibleSection(QStringLiteral("素材"));
        materialSection->addWidget(labeledRow(
            QStringLiteral("视频素材"),
            pathRow(inputEdit_, QStringLiteral("选择视频"),
                    QStringLiteral("视频 (*.mov *.mp4 *.mxf *.r3d)"))));
        source_ = new QComboBox;
        source_->addItem(QStringLiteral("Apple Log（iPhone）"), QStringLiteral("apple-log"));
        source_->addItem(QStringLiteral("Sony S-Log3（S-Gamut3.Cine）"), QStringLiteral("s-log3"));
        source_->addItem(QStringLiteral("Sony S-Log3（S-Gamut3）"),
                         QStringLiteral("s-log3-sgamut3"));
        source_->addItem(QStringLiteral("Canon C-Log3"), QStringLiteral("c-log3"));
        source_->addItem(QStringLiteral("Panasonic V-Log"), QStringLiteral("v-log"));
        source_->addItem(QStringLiteral("ARRI LogC3"), QStringLiteral("logc3"));
        source_->addItem(QStringLiteral("ARRI LogC4"), QStringLiteral("logc4"));
        source_->addItem(QStringLiteral("Blackmagic Film Gen 5（BMPCC）"),
                         QStringLiteral("bmd-film"));
        source_->addItem(QStringLiteral("DaVinci Intermediate（DWG）"),
                         QStringLiteral("davinci"));
        source_->addItem(QStringLiteral("普通 SDR 素材"), QStringLiteral("srgb"));
        materialSection->addWidget(labeledRow(QStringLiteral("画面类型"), source_));
        redScale_ = new QComboBox;
        redScale_->addItem(QStringLiteral("完整（full）"), QStringLiteral("full"));
        redScale_->addItem(QStringLiteral("半分辨率（half）"), QStringLiteral("half"));
        redScale_->addItem(QStringLiteral("四分之一（quarter）"), QStringLiteral("quarter"));
        redScale_->setCurrentIndex(1);
        redScale_->setToolTip(
            QStringLiteral("R3D 解码尺寸；6K 全分辨率很慢，预览/批量建议 half。"));
        materialSection->addWidget(labeledRow(QStringLiteral("R3D 解码"), redScale_));
        materialSection->addWidget(labeledRow(
            QStringLiteral("ffmpeg"),
            pathRow(ffmpegEdit_, QStringLiteral("选择 ffmpeg"),
                    QStringLiteral("ffmpeg (ffmpeg.exe)"))));
        leftLayout->addWidget(materialSection);

        // D063: OpenColorIO as an input colour space. The UI never links OCIO
        // (its Qt toolchain cannot link the OCIO static library); it asks the
        // CLI for the names and hands the decode to `lut bake` / the spectral
        // engine, exactly like the command line.
        auto* ocioSection = new CollapsibleSection(QStringLiteral("色彩管理（OCIO）"));
        ocioEnabled_ = new QCheckBox(QStringLiteral("用 OCIO 色彩空间代替“画面类型”"));
        ocioEnabled_->setToolTip(QStringLiteral(
            "勾选后，LUT 的输入域来自选中的 OCIO 颜色空间（相机 Log、ACES 等）。"
            "解码会烘进 LUT，渲染时不需要 OCIO。"));
        ocioSection->addWidget(ocioEnabled_);

        auto* ocioConfigRow = new QWidget;
        auto* ocioConfigLayout = new QHBoxLayout(ocioConfigRow);
        ocioConfigLayout->setContentsMargins(0, 0, 0, 0);
        ocioConfigLayout->setSpacing(4);
        ocioConfig_ = new QLineEdit;
        ocioConfigLayout->addWidget(ocioConfig_, 1);
        auto* ocioConfigBrowse = new QPushButton(QStringLiteral("…"));
        ocioConfigBrowse->setFixedWidth(28);
        connect(ocioConfigBrowse, &QPushButton::clicked, this, [this] {
            const QString file = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择 OCIO 配置"), ocioConfig_->text(),
                QStringLiteral("OCIO (*.ocio)"));
            if (file.isEmpty()) return;
            ocioConfig_->setText(QDir::toNativeSeparators(file));
            refreshOcioSpaces(false);
        });
        ocioConfigLayout->addWidget(ocioConfigBrowse);
        ocioRefreshButton_ = new QPushButton(QStringLiteral("读取"));
        connect(ocioRefreshButton_, &QPushButton::clicked, this,
                [this] { refreshOcioSpaces(true); });
        ocioConfigLayout->addWidget(ocioRefreshButton_);
        ocioSection->addWidget(labeledRow(QStringLiteral("配置文件"), ocioConfigRow));

        ocioSpace_ = new QComboBox;
        ocioSection->addWidget(labeledRow(QStringLiteral("色彩空间"), ocioSpace_));
        ocioInfoLabel_ = noteLabel(QStringLiteral("勾选后点“读取”，从配置里列出所有色彩空间。"));
        ocioSection->addWidget(ocioInfoLabel_);
        connect(ocioConfig_, &QLineEdit::editingFinished, this, [this] {
            if (ocioEnabled_->isChecked()) refreshOcioSpaces(false);
        });
        leftLayout->addWidget(ocioSection);

        auto* filmSection = new CollapsibleSection(QStringLiteral("胶片与相纸"));
        auto* lutRow = new QWidget;
        auto* lutLayout = new QHBoxLayout(lutRow);
        lutLayout->setContentsMargins(0, 0, 0, 0);
        lutLayout->setSpacing(4);
        lutLayout->addWidget(lutEdit_, 1);
        auto* lutBrowse = new QPushButton(QStringLiteral("…"));
        lutBrowse->setFixedWidth(28);
        lutBrowseButton_ = lutBrowse;
        connect(lutBrowse, &QPushButton::clicked, this, [this] {
            const QString file = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择 LUT"), lutEdit_->text(), QStringLiteral("LUT (*.cube)"));
            if (!file.isEmpty()) {
                lutEdit_->setText(QDir::toNativeSeparators(file));
                lutBaked_ = false;
                lutStale_ = false;
            }
        });
        lutLayout->addWidget(lutBrowse);
        bakeButton_ = new QPushButton(QStringLiteral("烘焙"));
        connect(bakeButton_, &QPushButton::clicked, this, [this] { bakeLut(); });
        lutLayout->addWidget(bakeButton_);
        filmSection->addWidget(labeledRow(QStringLiteral("LUT"), lutRow));
        filmSection->addWidget(noteLabel(
            QStringLiteral("曝光、扫描方式和反差在右侧“参数”栏；改完点烘焙，或直接预览时会自动更新。")));
        leftLayout->addWidget(filmSection);

        auto* outputSection = new CollapsibleSection(QStringLiteral("输出"));
        outputSection->addWidget(labeledRow(
            QStringLiteral("输出文件"),
            saveRow(outputEdit_, QStringLiteral("保存视频"), QStringLiteral("视频 (*.mov *.mp4)"))));
        engine_ = new QComboBox;
        engine_->addItem(QStringLiteral("LUT（快）"), QStringLiteral("lut"));
        engine_->addItem(QStringLiteral("光谱（物理，慢）"), QStringLiteral("spectral"));
        engine_->addItem(QStringLiteral("光谱 GPU（物理，快）"), QStringLiteral("spectral-gpu"));
        engine_->setToolTip(
            QStringLiteral("LUT：每帧查表，实时；光谱：逐像素物理链，约 1.2 秒 / 2.6 MP；"
                           "光谱 GPU：同一物理链在显卡上跑，约 50 毫秒 / 2.6 MP"));
        outputSection->addWidget(labeledRow(QStringLiteral("引擎"), engine_));
        textureGpu_ = new QCheckBox(QStringLiteral("GPU 质感（颗粒 / 光晕 / 眩光 / 扩散）"));
        textureGpu_->setChecked(true);
        textureGpu_->setToolTip(QStringLiteral(
            "把空间质感放到显卡上跑（D047）；取消勾选用 CPU 参考链。没有 D3D11 显卡时自动回退。"));
        outputSection->addWidget(textureGpu_);
        codec_ = new QComboBox;
        codec_->addItem(QStringLiteral("H.264"), QStringLiteral("h264"));
        codec_->addItem(QStringLiteral("HEVC（HDR 推荐）"), QStringLiteral("hevc"));
        codec_->addItem(QStringLiteral("ProRes 422 HQ"), QStringLiteral("prores"));
        outputSection->addWidget(labeledRow(QStringLiteral("编码"), codec_));
        trc_ = new QComboBox;
        trc_->addItem(QStringLiteral("SDR"), QStringLiteral("srgb"));
        trc_->addItem(QStringLiteral("HDR · PQ"), QStringLiteral("pq"));
        trc_->addItem(QStringLiteral("HDR · HLG"), QStringLiteral("hlg"));
        outputSection->addWidget(labeledRow(QStringLiteral("色彩"), trc_));
        start_ = new QDoubleSpinBox;
        start_->setRange(0.0, 86400.0);
        start_->setDecimals(3);
        start_->setSuffix(QStringLiteral(" s"));
        outputSection->addWidget(labeledRow(QStringLiteral("起始"), start_));
        frames_ = new QSpinBox;
        frames_->setRange(0, 10000000);
        frames_->setSpecialValueText(QStringLiteral("整段"));
        outputSection->addWidget(labeledRow(QStringLiteral("帧数"), frames_));
        segment_ = new QSpinBox;
        segment_->setRange(0, 3600);
        segment_->setSuffix(QStringLiteral(" s"));
        segment_->setSpecialValueText(QStringLiteral("不分段"));
        segment_->setValue(10);
        outputSection->addWidget(labeledRow(QStringLiteral("分段长度"), segment_));
        resume_ = new QCheckBox(QStringLiteral("中断后续渲（跳过已完成的段）"));
        resume_->setChecked(true);
        outputSection->addWidget(resume_);
        leftLayout->addWidget(outputSection);
        leftLayout->addStretch(1);

        auto* leftScroll = new QScrollArea;
        leftScroll->setWidgetResizable(true);
        leftScroll->setWidget(left);
        leftScroll->setMinimumWidth(238);
        leftScroll->setMaximumWidth(310);

        // ---- centre: canvas, metadata, timeline, status --------------------
        preview_ = new PreviewView;
        clipInfoLabel_ = new QLabel(QStringLiteral("未选择素材"));
        clipInfoLabel_->setObjectName(QStringLiteral("muted"));
        clipInfoLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

        timeline_ = new QSlider(Qt::Horizontal);
        timeline_->setRange(0, 0);
        timeline_->setToolTip(QStringLiteral("拖动到某一帧，松开后刷新预览"));
        timeLabel_ = new QLabel(QStringLiteral("--"));
        timeLabel_->setObjectName(QStringLiteral("muted"));
        timeLabel_->setMinimumWidth(180);
        timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        status_ = new QLabel(QStringLiteral("就绪"));
        status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        progress_ = new QProgressBar;
        progress_->setRange(0, 100);
        progress_->setValue(0);
        progress_->setTextVisible(true);
        progress_->setFixedWidth(180);
        progress_->setFixedHeight(14);

        auto* infoBar = new QWidget;
        infoBar->setObjectName(QStringLiteral("infoBar"));
        infoBar->setFixedHeight(28);
        auto* infoLayout = new QHBoxLayout(infoBar);
        infoLayout->setContentsMargins(12, 0, 12, 0);
        infoLayout->addWidget(clipInfoLabel_, 1);

        auto* timelineBar = new QWidget;
        timelineBar->setObjectName(QStringLiteral("infoBar"));
        timelineBar->setFixedHeight(34);
        auto* timelineLayout = new QHBoxLayout(timelineBar);
        timelineLayout->setContentsMargins(12, 4, 12, 4);
        timelineLayout->addWidget(timeline_, 1);
        timelineLayout->addWidget(timeLabel_);

        auto* statusBar = new QWidget;
        statusBar->setObjectName(QStringLiteral("infoBar"));
        statusBar->setFixedHeight(30);
        auto* statusLayout = new QHBoxLayout(statusBar);
        statusLayout->setContentsMargins(12, 0, 12, 0);
        statusLayout->addWidget(status_, 1);
        statusLayout->addWidget(progress_);

        auto* centre = new QWidget;
        centre->setObjectName(QStringLiteral("centre"));
        auto* centreLayout = new QVBoxLayout(centre);
        centreLayout->setContentsMargins(0, 0, 0, 0);
        centreLayout->setSpacing(0);
        centreLayout->addWidget(preview_, 1);
        centreLayout->addWidget(infoBar);
        centreLayout->addWidget(timelineBar);
        centreLayout->addWidget(statusBar);
        centre->setMinimumWidth(480);

        // ---- right rail: pre-dev / post-dev / queue / log ------------------
        auto* exposureRow = new SliderRow(QStringLiteral("曝光"), -6.0, 6.0, 0.1, 2, 0.7,
                                          QStringLiteral(" EV"));
        exposure_ = exposureRow->spin();
        couplers_ = new QCheckBox(QStringLiteral("DIR 耦合剂（显影抑制）"));
        couplers_->setChecked(true);
        couplers_->setToolTip(
            QStringLiteral("层间抑制：让饱和色更容易分开。Apple Log 默认开，sRGB 参考默认关。"));
        auto* grainRow = new SliderRow(QStringLiteral("颗粒"), 0.0, 1.0, 0.05, 2, 0.4);
        grain_ = grainRow->spin();
        grainRate_ = new QSpinBox;
        grainRate_->setRange(1, 12);
        grainRate_->setValue(3);
        grainRate_->setToolTip(QStringLiteral("一个颗粒场持续几帧；3 最接近真胶片，1 会闪。"));
        auto* halationRow = new SliderRow(QStringLiteral("光晕"), 0.0, 1.0, 0.05, 2, 0.45);
        halation_ = halationRow->spin();

        scan_ = new QComboBox;
        scan_->addItem(QStringLiteral("扫描负片（推荐）"), QStringLiteral("negative"));
        scan_->addItem(QStringLiteral("扫描反转片（直扫）"), QStringLiteral("slide"));
        scan_->addItem(QStringLiteral("印片"), QStringLiteral("print"));
        scan_->setToolTip(
            QStringLiteral("负片：倒相扫描；反转片：直扫已显影的正像；印片：负片印到相纸/电影印片。"));
        auto* contrastRow = new SliderRow(QStringLiteral("反差"), 0.5, 2.0, 0.05, 2, 1.15);
        contrast_ = contrastRow->spin();
        gamut_ = new QCheckBox(QStringLiteral("输出色域压缩（OkLCh）"));
        gamut_->setChecked(true);
        gamut_->setToolTip(
            QStringLiteral("把超出 sRGB 的颜色按 OkLCh 降饱和度收回来；Apple Log 默认开。"));
        auto* glareRow = new SliderRow(QStringLiteral("眩光"), 0.0, 1.0, 0.05, 2, 0.2);
        glare_ = glareRow->spin();
        auto* diffusionRow = new SliderRow(QStringLiteral("扩散"), 0.0, 1.0, 0.05, 2, 0.1);
        diffusion_ = diffusionRow->spin();
        previewTime_ = new QDoubleSpinBox;
        previewTime_->setRange(0.0, 86400.0);
        previewTime_->setDecimals(2);
        previewTime_->setSuffix(QStringLiteral(" s"));
        previewTime_->setValue(1.5);

        auto* exposureSection = new CollapsibleSection(QStringLiteral("曝光"));
        exposureSection->addWidget(exposureRow);
        exposureSection->addWidget(noteLabel(
            QStringLiteral("曝光发生在进入底片之前，决定场景落在特性曲线的哪一段。")));

        auto* negativeSection = new CollapsibleSection(QStringLiteral("底片"));
        filmCombo_ = new QComboBox;
        filmCombo_->setToolTip(
            QStringLiteral("彩色负片、Vision3 电影负片和反转片（Ektachrome/Velvia/Provia/Kodachrome）。"));
        negativeSection->addWidget(labeledRow(QStringLiteral("胶片"), filmCombo_));
        negativeSection->addWidget(couplers_);
        negativeSection->addWidget(noteLabel(
            QStringLiteral("DIR 耦合剂属于显影阶段；它不会改变颗粒，只影响层间抑制。")));

        auto* emulsionSection = new CollapsibleSection(QStringLiteral("乳剂与背层"));
        emulsionSection->addWidget(grainRow);
        emulsionSection->addWidget(labeledRow(QStringLiteral("颗粒稳定度"), grainRate_));
        emulsionSection->addWidget(halationRow);
        emulsionSection->addWidget(noteLabel(
            QStringLiteral("颗粒和光晕属于底片本身：颗粒在乳剂里，光晕来自背面层。")));

        auto* preScroll = new QScrollArea;
        preScroll->setWidgetResizable(true);
        auto* pre = new QWidget;
        auto* preLayout = new QVBoxLayout(pre);
        preLayout->setContentsMargins(0, 0, 0, 0);
        preLayout->setSpacing(0);
        auto* preTitle = new QLabel(QStringLiteral("显影前 · 底片"));
        preTitle->setObjectName(QStringLiteral("railTitle"));
        preTitle->setFixedHeight(42);
        preTitle->setContentsMargins(14, 0, 14, 0);
        preLayout->addWidget(preTitle);
        preLayout->addWidget(exposureSection);
        preLayout->addWidget(negativeSection);
        preLayout->addWidget(emulsionSection);
        preLayout->addStretch(1);
        preScroll->setWidget(pre);

        auto* printSection = new CollapsibleSection(QStringLiteral("印片与扫描"));
        paperCombo_ = new QComboBox;
        paperCombo_->setToolTip(
            QStringLiteral("印片相纸或电影印片；反转片直扫不需要印片，选择会自动停用。"));
        printSection->addWidget(labeledRow(QStringLiteral("相纸"), paperCombo_));
        printSection->addWidget(labeledRow(QStringLiteral("扫描方式"), scan_));
        printSection->addWidget(contrastRow);
        printSection->addWidget(gamut_);
        printSection->addWidget(noteLabel(
            QStringLiteral("显影后：密度被印到相纸或直接扫描。扫描负片保留更多高光；反差在扫描后应用。")));

        auto* lensSection = new CollapsibleSection(QStringLiteral("镜头与扫描扩散"));
        lensSection->addWidget(glareRow);
        lensSection->addWidget(diffusionRow);
        lensSection->addWidget(noteLabel(
            QStringLiteral("眩光和扩散属于镜头/扫描环节，不改变底片和相纸的颜色。")));

        auto* viewSection = new CollapsibleSection(QStringLiteral("视图"));
        viewSection->addWidget(labeledRow(QStringLiteral("预览时间"), previewTime_));
        viewSection->addWidget(noteLabel(
            QStringLiteral("拖动画布下方时间线换帧；滚轮缩放，双击切换适合/100%。")));

        auto* postScroll = new QScrollArea;
        postScroll->setWidgetResizable(true);
        auto* post = new QWidget;
        auto* postLayout = new QVBoxLayout(post);
        postLayout->setContentsMargins(0, 0, 0, 0);
        postLayout->setSpacing(0);
        auto* postTitle = new QLabel(QStringLiteral("显影后 · 印片与扫描"));
        postTitle->setObjectName(QStringLiteral("railTitle"));
        postTitle->setFixedHeight(42);
        postTitle->setContentsMargins(14, 0, 14, 0);
        postLayout->addWidget(postTitle);
        postLayout->addWidget(printSection);
        postLayout->addWidget(lensSection);
        postLayout->addWidget(viewSection);
        postLayout->addStretch(1);
        postScroll->setWidget(post);
        populateStocks();

        log_ = new QPlainTextEdit;
        log_->setReadOnly(true);
        log_->setMaximumBlockCount(4000);
        log_->setPlaceholderText(QStringLiteral("渲染日志会显示在这里"));
        auto* logTab = new QWidget;
        auto* logLayout = new QVBoxLayout(logTab);
        logLayout->setContentsMargins(8, 8, 8, 8);
        logLayout->addWidget(log_);

        queueTable_ = new QTableWidget(0, 5);
        queueTable_->setHorizontalHeaderLabels({QStringLiteral("素材"), QStringLiteral("输出"),
                                                QStringLiteral("状态"), QStringLiteral("进度"),
                                                QStringLiteral("用时 / 剩余")});
        queueTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        queueTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        queueTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        queueTable_->verticalHeader()->setVisible(false);
        queueTable_->verticalHeader()->setDefaultSectionSize(34);
        queueTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        queueTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        queueTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        queueTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
        queueTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        queueTable_->setColumnWidth(3, 120);

        auto* queueButtons = new QHBoxLayout;
        queueStartButton_ = new QPushButton(QStringLiteral("开始队列"));
        queueStartButton_->setProperty("accent", true);
        queueStopButton_ = new QPushButton(QStringLiteral("停止"));
        batchAddButton_ = new QPushButton(QStringLiteral("批量加入"));
        batchAddButton_->setToolTip(QStringLiteral("一次选择多个视频，按当前参数和 LUT 排成多条任务"));
        queueRemoveButton_ = new QPushButton(QStringLiteral("移除"));
        queueClearButton_ = new QPushButton(QStringLiteral("清空完成"));
        queueOpenButton_ = new QPushButton(QStringLiteral("打开输出"));
        queueButtons->addWidget(queueStartButton_);
        queueButtons->addWidget(queueStopButton_);
        queueButtons->addWidget(batchAddButton_);
        queueButtons->addStretch(1);
        queueButtons->addWidget(queueRemoveButton_);
        queueButtons->addWidget(queueClearButton_);
        queueButtons->addWidget(queueOpenButton_);

        auto* queueTab = new QWidget;
        auto* queueLayout = new QVBoxLayout(queueTab);
        queueLayout->setContentsMargins(8, 8, 8, 8);
        queueLayout->addWidget(queueTable_, 1);
        queueLayout->addLayout(queueButtons);

        tabs_ = new QTabWidget;
        tabs_->addTab(preScroll, QStringLiteral("显影前"));
        tabs_->addTab(postScroll, QStringLiteral("显影后"));
        tabs_->addTab(queueTab, QStringLiteral("队列"));
        tabs_->addTab(logTab, QStringLiteral("日志"));
        tabs_->setMinimumWidth(300);
        tabs_->setMaximumWidth(410);
        queueTabIndex_ = 2;

        connect(previewButton_, &QPushButton::clicked, this, [this] { previewFrame(); });
        connect(renderButton_, &QPushButton::clicked, this, [this] { startRender(); });
        connect(addQueueButton_, &QPushButton::clicked, this, [this] { enqueueJob(); });
        connect(cancelButton_, &QPushButton::clicked, this, [this] { cancelRender(); });
        connect(compareButton_, &QPushButton::pressed, this, [this] { showOriginal(); });
        connect(compareButton_, &QPushButton::released, this, [this] { restoreGraded(); });
        connect(folderButton, &QPushButton::clicked, this, [this] { openOutputFolder(); });
        connect(inputEdit_, &QLineEdit::editingFinished, this, [this] { probeInput(); });
        connect(ffmpegEdit_, &QLineEdit::editingFinished, this, [this] { probeInput(); });
        auto* probeTimer = new QTimer(this);
        probeTimer->setSingleShot(true);
        probeTimer->setInterval(350);
        connect(inputEdit_, &QLineEdit::textChanged, probeTimer,
                [probeTimer] { probeTimer->start(); });
        connect(ffmpegEdit_, &QLineEdit::textChanged, probeTimer,
                [probeTimer] { probeTimer->start(); });
        connect(probeTimer, &QTimer::timeout, this, [this] { probeInput(); });

        auto* previewShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+P")), this);
        connect(previewShortcut, &QShortcut::activated, this, [this] { previewFrame(); });
        auto* renderShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+R")), this);
        connect(renderShortcut, &QShortcut::activated, this, [this] { startRender(); });
        auto* bakeShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+B")), this);
        connect(bakeShortcut, &QShortcut::activated, this, [this] { bakeLut(); });
        auto* fitShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+0")), this);
        connect(fitShortcut, &QShortcut::activated, this, [this] { preview_->fitToWindow(); });
        auto* zoomShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+1")), this);
        connect(zoomShortcut, &QShortcut::activated, this, [this] { preview_->setZoom(1.0); });

        preview_->setZoomCallback([this](double percent, bool fit) {
            zoomLabel_->setText(percent > 0.0 ? QString::number(percent, 'f', 0) + QLatin1Char('%')
                                              : QStringLiteral("—"));
            fitButton_->setChecked(fit);
            zoom100Button_->setChecked(!fit && std::abs(percent - 100.0) < 0.5);
        });
        connect(fitButton_, &QPushButton::clicked, this, [this] { preview_->fitToWindow(); });
        connect(zoom100Button_, &QPushButton::clicked, this, [this] { preview_->setZoom(1.0); });

        // Changing a bake parameter makes an auto-baked LUT stale; preview and
        // render rebake it once before running. A user-chosen LUT is left alone.
        connect(lutEdit_, &QLineEdit::textEdited, this, [this] {
            lutBaked_ = false;
            lutStale_ = false;
        });
        connect(exposure_, &QDoubleSpinBox::valueChanged, this,
                [this](double) { markLookChanged(); });
        connect(contrast_, &QDoubleSpinBox::valueChanged, this,
                [this](double) { markLookChanged(); });
        connect(couplers_, &QCheckBox::toggled, this, [this](bool) { markLookChanged(); });
        connect(gamut_, &QCheckBox::toggled, this, [this](bool) { markLookChanged(); });
        connect(source_, &QComboBox::currentIndexChanged, this, [this](int) {
            originalImage_ = QImage();
            originalTime_ = -1.0;
            const bool isLog =
                source_->currentData().toString() != QStringLiteral("srgb");
            couplers_->setChecked(isLog);
            gamut_->setChecked(isLog);
            markLookChanged();
        });
        connect(ocioEnabled_, &QCheckBox::toggled, this, [this](bool on) {
            updateOcioUi();
            if (on) {
                if (ocioSpace_->count() == 0) refreshOcioSpaces(false);
                // An OCIO input is a camera log space in practice; mirror what
                // picking a built-in log does (D060).
                couplers_->setChecked(true);
                gamut_->setChecked(true);
            }
            originalImage_ = QImage();
            originalTime_ = -1.0;
            markLookChanged();
        });
        connect(ocioSpace_, &QComboBox::currentIndexChanged, this, [this](int) {
            originalImage_ = QImage();
            originalTime_ = -1.0;
            markLookChanged();
        });
        // D067: a reversal stock is scanned directly, so the paper controls are
        // irrelevant and the scan mode follows the stock type.
        const auto updateScanUi = [this]() {
            const bool slide = scan_->currentData().toString() == QStringLiteral("slide");
            if (paperCombo_ != nullptr) paperCombo_->setEnabled(!slide);
        };
        connect(redScale_, &QComboBox::currentIndexChanged, this,
                [this](int) { markLookChanged(); });
        connect(scan_, &QComboBox::currentIndexChanged, this,
                [this, updateScanUi](int) {
                    const bool positive = filmCombo_->currentData(Qt::UserRole + 2).toBool();
                    const QString mode = scan_->currentData().toString();
                    if (positive && mode != QStringLiteral("slide")) {
                        const QSignalBlocker blocker(scan_);
                        selectData(scan_, QStringLiteral("slide"));
                    } else if (!positive && mode == QStringLiteral("slide")) {
                        const QSignalBlocker blocker(scan_);
                        selectData(scan_, QStringLiteral("negative"));
                    }
                    updateScanUi();
                    markLookChanged();
                });
        connect(filmCombo_, &QComboBox::currentIndexChanged, this, [this, updateScanUi](int) {
            const QString target = filmCombo_->currentData(Qt::UserRole + 1).toString();
            if (!target.isEmpty()) {
                for (int i = 0; i < paperCombo_->count(); ++i) {
                    if (QFileInfo(paperCombo_->itemData(i).toString()).completeBaseName() ==
                        target) {
                        const QSignalBlocker blocker(paperCombo_);
                        paperCombo_->setCurrentIndex(i);
                        break;
                    }
                }
            }
            const bool positive = filmCombo_->currentData(Qt::UserRole + 2).toBool();
            const bool slide = scan_->currentData().toString() == QStringLiteral("slide");
            if (positive && !slide) {
                selectData(scan_, QStringLiteral("slide"));
                if (std::abs(contrast_->value() - 1.15) < 1e-6) contrast_->setValue(1.0);
            } else if (!positive && slide) {
                selectData(scan_, QStringLiteral("negative"));
                if (std::abs(contrast_->value() - 1.0) < 1e-6) contrast_->setValue(1.15);
            }
            updateScanUi();
            markLookChanged();
        });
        connect(paperCombo_, &QComboBox::currentIndexChanged, this,
                [this](int) { markLookChanged(); });
        connect(engine_, &QComboBox::currentIndexChanged, this, [this](int) {
            updateEngineUi();
            markLookChanged();
        });
        connect(trc_, &QComboBox::currentIndexChanged, this,
                [this](int) { markLookChanged(); });

        connect(queueStartButton_, &QPushButton::clicked, this, [this] { startQueue(); });
        connect(queueStopButton_, &QPushButton::clicked, this, [this] { stopQueue(); });
        connect(queueRemoveButton_, &QPushButton::clicked, this, [this] { removeSelectedJob(); });
        connect(queueClearButton_, &QPushButton::clicked, this, [this] { clearFinishedJobs(); });
        connect(queueOpenButton_, &QPushButton::clicked, this, [this] { openSelectedOutput(); });
        connect(batchAddButton_, &QPushButton::clicked, this, [this] {
            const QStringList files = QFileDialog::getOpenFileNames(
                this, QStringLiteral("选择多个视频"),
                QFileInfo(inputEdit_->text()).absolutePath(),
                QStringLiteral("视频 (*.mov *.mp4 *.mxf *.mkv *.r3d)"));
            enqueueBatch(files);
        });
        connect(queueTable_, &QTableWidget::itemSelectionChanged, this,
                [this] { updateQueueButtons(); });
        connect(queueTable_, &QTableWidget::cellDoubleClicked, this,
                [this](int, int) { openSelectedOutput(); });

        etaTimer_ = new QTimer(this);
        etaTimer_->setInterval(1000);
        connect(etaTimer_, &QTimer::timeout, this, [this] {
            if (activeJob_ >= 0 && activeJob_ < jobs_.size()) updateJobRow(activeJob_);
        });

        connect(timeline_, &QSlider::valueChanged, this, [this](int value) {
            if (clipInfo_.fps > 0.0) {
                const QSignalBlocker blocker(previewTime_);
                previewTime_->setValue(value / clipInfo_.fps);
            }
            updateTimeLabel();
        });
        // Scrubbing: coalesce moves into ~11 previews/s while the handle is
        // held, then one final frame on release. Each preview is ~50 ms now, so
        // the handle still tracks the pointer between renders.
        scrubTimer_ = new QTimer(this);
        scrubTimer_->setSingleShot(true);
        scrubTimer_->setInterval(90);
        connect(scrubTimer_, &QTimer::timeout, this, [this] {
            if (scrubbing_) previewFrame();
        });
        connect(timeline_, &QSlider::sliderMoved, this, [this] {
            scrubbing_ = true;
            scrubTimer_->start();
        });
        connect(timeline_, &QSlider::sliderReleased, this, [this] {
            scrubbing_ = false;
            scrubTimer_->stop();
            previewFrame();
        });
        connect(previewTime_, &QDoubleSpinBox::editingFinished, this,
                [this] { syncTimelineFromTime(); });
        connect(previewTime_, &QDoubleSpinBox::valueChanged, this,
                [this](double) { syncTimelineFromTime(); });

        auto* splitter = new QSplitter(Qt::Horizontal);
        splitter->addWidget(leftScroll);
        splitter->addWidget(centre);
        splitter->addWidget(tabs_);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setStretchFactor(2, 0);
        splitter->setSizes({250, 820, 330});
        splitter->setChildrenCollapsible(false);

        // The queue needs a wider rail than the parameter inspector.
        connect(tabs_, &QTabWidget::currentChanged, this, [this, splitter](int index) {
            const bool queueTab = index == queueTabIndex_;
            const bool logTab = index == queueTabIndex_ + 1;
            tabs_->setMinimumWidth(queueTab ? 480 : (logTab ? 380 : 300));
            tabs_->setMaximumWidth(queueTab ? 700 : 410);
            QList<int> sizes = splitter->sizes();
            const int target = queueTab ? 500 : (logTab ? 380 : 330);
            sizes[1] += sizes[2] - target;
            sizes[2] = target;
            if (sizes[1] < 420) {
                sizes[0] = qMax(200, sizes[0] - (420 - sizes[1]));
                sizes[1] = 420;
            }
            splitter->setSizes(sizes);
        });

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(toolbar);
        layout->addWidget(splitter, 1);
    }

    // ---- settings --------------------------------------------------------

    void loadSettings() {
        QSettings settings(QStringLiteral("SpektraRoll"), QStringLiteral("SpektraRoll UI"));
        const QString defaultLut = repoRoot_ + QStringLiteral("/out/ui-portra400.cube");
        // D065: the bundled test clip is now the Sony FX30 S-Log3 recording.
        // Migrate the old default so an existing install does not decode the
        // new clip with the Apple Log curve.
        QString storedInput =
            settings.value(QStringLiteral("paths/input"), defaultInput()).toString();
        QString storedSource =
            settings.value(QStringLiteral("look/source"), QStringLiteral("s-log3")).toString();
        if (storedInput.contains(QStringLiteral("A001_11011538_C001"))) {
            storedInput = defaultInput();
            if (storedSource == QStringLiteral("apple-log")) storedSource = QStringLiteral("s-log3");
        }
        inputEdit_->setText(storedInput);
        ffmpegEdit_->setText(
            settings.value(QStringLiteral("paths/ffmpeg"), defaultFfmpeg()).toString());
        lutEdit_->setText(settings.value(QStringLiteral("paths/lut"), defaultLut).toString());
        outputEdit_->setText(settings
                                 .value(QStringLiteral("paths/output"),
                                        repoRoot_ + QStringLiteral("/out/ui-render.mov"))
                                 .toString());

        selectData(source_, storedSource);
        const auto selectStock = [](QComboBox* combo, const QString& fileName) {
            for (int i = 0; i < combo->count(); ++i) {
                if (QFileInfo(combo->itemData(i).toString()).fileName() == fileName) {
                    combo->setCurrentIndex(i);
                    return;
                }
            }
        };
        selectStock(filmCombo_,
                    settings.value(QStringLiteral("look/film"),
                                   QStringLiteral("kodak_portra_400.json")).toString());
        selectStock(paperCombo_,
                    settings.value(QStringLiteral("look/paper"),
                                   QStringLiteral("kodak_portra_endura.json")).toString());
        exposure_->setValue(settings.value(QStringLiteral("look/exposure"), 0.7).toDouble());
        selectData(redScale_,
                   settings.value(QStringLiteral("look/redScale"), QStringLiteral("half")).toString());
        selectData(scan_, settings.value(QStringLiteral("look/scan"), QStringLiteral("negative")).toString());
        contrast_->setValue(settings.value(QStringLiteral("look/contrast"), 1.15).toDouble());
        couplers_->setChecked(settings.value(QStringLiteral("look/couplers"), true).toBool());
        gamut_->setChecked(settings.value(QStringLiteral("look/gamut"), true).toBool());
        selectData(trc_, settings.value(QStringLiteral("look/trc"), QStringLiteral("srgb")).toString());

        grain_->setValue(settings.value(QStringLiteral("texture/grain"), 0.4).toDouble());
        grainRate_->setValue(settings.value(QStringLiteral("texture/grainRate"), 3).toInt());
        halation_->setValue(settings.value(QStringLiteral("texture/halation"), 0.45).toDouble());
        glare_->setValue(settings.value(QStringLiteral("texture/glare"), 0.2).toDouble());
        diffusion_->setValue(settings.value(QStringLiteral("texture/diffusion"), 0.1).toDouble());
        if (textureGpu_ != nullptr) {
            textureGpu_->setChecked(settings.value(QStringLiteral("texture/gpu"), true).toBool());
        }

        selectData(codec_, settings.value(QStringLiteral("render/codec"), QStringLiteral("h264")).toString());
        selectData(engine_, settings.value(QStringLiteral("render/engine"), QStringLiteral("lut")).toString());
        start_->setValue(settings.value(QStringLiteral("render/start"), 0.0).toDouble());
        frames_->setValue(settings.value(QStringLiteral("render/frames"), 0).toInt());
        segment_->setValue(settings.value(QStringLiteral("render/segment"), 10).toInt());
        resume_->setChecked(settings.value(QStringLiteral("render/resume"), true).toBool());
        previewTime_->setValue(settings.value(QStringLiteral("preview/time"), 1.5).toDouble());

        // D063: OCIO input. Restore the config path, then fill the space list
        // from the CLI. Disabled by default so the built-in inputs are unchanged.
        ocioConfig_->setText(
            settings.value(QStringLiteral("ocio/config"), defaultOcioConfig()).toString());
        const bool ocioWasOn = settings.value(QStringLiteral("ocio/enabled"), false).toBool();
        {
            const QSignalBlocker blocker(ocioEnabled_);
            ocioEnabled_->setChecked(ocioWasOn);
        }
        updateOcioUi();
        if (ocioWasOn) {
            refreshOcioSpaces(false);
            selectData(ocioSpace_, settings.value(QStringLiteral("ocio/space")).toString());
        }

        const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
        if (!geometry.isEmpty()) {
            restoreGeometry(geometry);
        } else {
            resize(1200, 760);
        }

        // Show the start of long paths instead of their tail.
        for (QLineEdit* edit : {inputEdit_, ffmpegEdit_, lutEdit_, outputEdit_}) {
            edit->setCursorPosition(0);
        }
        updateEngineUi();

        // If the LUT was baked by this UI, remember whether the controls still
        // match it; otherwise preview/render would silently use an old look.
        QFile params(lutEdit_->text().trimmed() + QStringLiteral(".params"));
        if (params.exists() && params.open(QIODevice::ReadOnly)) {
            lutBaked_ = true;
            lutStale_ = QString::fromUtf8(params.readAll()).trimmed() != lookSignature();
            if (lutStale_) {
                status_->setText(
                    QStringLiteral("LUT 参数与界面不一致：预览或渲染时会重新烘焙"));
            }
        }
    }

    void saveSettings() {
        QSettings settings(QStringLiteral("SpektraRoll"), QStringLiteral("SpektraRoll UI"));
        settings.setValue(QStringLiteral("paths/input"), inputEdit_->text());
        settings.setValue(QStringLiteral("paths/ffmpeg"), ffmpegEdit_->text());
        settings.setValue(QStringLiteral("paths/lut"), lutEdit_->text());
        settings.setValue(QStringLiteral("paths/output"), outputEdit_->text());
        settings.setValue(QStringLiteral("look/source"), source_->currentData().toString());
        settings.setValue(QStringLiteral("look/film"),
                          QFileInfo(filmCombo_->currentData().toString()).fileName());
        settings.setValue(QStringLiteral("look/paper"),
                          QFileInfo(paperCombo_->currentData().toString()).fileName());
        settings.setValue(QStringLiteral("look/exposure"), exposure_->value());
        settings.setValue(QStringLiteral("look/redScale"), redScale_->currentData().toString());
        settings.setValue(QStringLiteral("look/scan"), scan_->currentData().toString());
        settings.setValue(QStringLiteral("look/contrast"), contrast_->value());
        settings.setValue(QStringLiteral("look/couplers"), couplers_->isChecked());
        settings.setValue(QStringLiteral("look/gamut"), gamut_->isChecked());
        settings.setValue(QStringLiteral("look/trc"), trc_->currentData().toString());
        settings.setValue(QStringLiteral("ocio/enabled"), ocioActive());
        settings.setValue(QStringLiteral("ocio/config"), ocioConfig_->text());
        settings.setValue(QStringLiteral("ocio/space"), ocioSpace_->currentData().toString());
        settings.setValue(QStringLiteral("texture/grain"), grain_->value());
        settings.setValue(QStringLiteral("texture/grainRate"), grainRate_->value());
        settings.setValue(QStringLiteral("texture/halation"), halation_->value());
        settings.setValue(QStringLiteral("texture/glare"), glare_->value());
        settings.setValue(QStringLiteral("texture/diffusion"), diffusion_->value());
        settings.setValue(QStringLiteral("texture/gpu"),
                          textureGpu_ != nullptr && textureGpu_->isChecked());
        settings.setValue(QStringLiteral("render/codec"), codec_->currentData().toString());
        settings.setValue(QStringLiteral("render/engine"), engine_->currentData().toString());
        settings.setValue(QStringLiteral("render/start"), start_->value());
        settings.setValue(QStringLiteral("render/frames"), frames_->value());
        settings.setValue(QStringLiteral("render/segment"), segment_->value());
        settings.setValue(QStringLiteral("render/resume"), resume_->isChecked());
        settings.setValue(QStringLiteral("preview/time"), previewTime_->value());
        settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    }

    static void selectData(QComboBox* combo, const QString& data) {
        const int index = combo->findData(data);
        if (index >= 0) combo->setCurrentIndex(index);
    }

    // ---- RED R3D (D068) ---------------------------------------------------

    static bool isRedPath(const QString& path) {
        return QFileInfo(path).suffix().compare(QStringLiteral("r3d"), Qt::CaseInsensitive) == 0;
    }

    // `red-info --json` is the only thing the UI knows about RED: the SDK is
    // never linked into the Qt binary. Without the optional local build it
    // reports built=false and the UI explains the REDCINE-X route.
    QJsonObject queryRedInfo(const QString& path = QString()) {
        QJsonObject result;
        if (cliPath_.trimmed().isEmpty() || !QFileInfo::exists(cliPath_)) return result;
        QStringList args{QStringLiteral("red-info"), QStringLiteral("--json")};
        if (!path.isEmpty()) args << QStringLiteral("--in") << path;
        QString output;
        if (!runSync(cliPath_, args, &output)) return result;
        const qsizetype start = output.indexOf(QLatin1Char('{'));
        if (start < 0) return result;
        const QJsonDocument document = QJsonDocument::fromJson(output.mid(start).toUtf8());
        if (!document.isObject()) return result;
        return document.object();
    }

    QString redBlockMessage(const QString& path) {
        const QJsonObject red = queryRedInfo(path);
        const QString sdkMessage = red.value(QStringLiteral("message")).toString();
        if (!red.value(QStringLiteral("available")).toBool()) {
            return QStringLiteral("这个构建没有可用的 RED R3D 支持。\n%1\n\n"
                                  "先用 REDCINE-X 导出 ProRes 4444（REDWideGamutRGB + "
                                  "Log3G10），或按 docs/red-sdk.md 用你自己的 SDK 本地编译。")
                .arg(sdkMessage.isEmpty()
                         ? QStringLiteral("SDK 未编译或初始化失败。")
                         : sdkMessage);
        }
        const QJsonObject clip = red.value(QStringLiteral("clip")).toObject();
        if (!clip.isEmpty()) {
            return QStringLiteral("R3D 元数据已读到（%1×%2，%3 帧）。\n"
                                  "CLI 已经能直接渲染：`video --in clip.R3D`（LUT 要用 "
                                  "Log3G10/RWG 烘），或 `red-decode` + `lut apply`。"
                                  "界面里的预览/渲染直连是下一阶段。")
                .arg(clip.value(QStringLiteral("width")).toInt())
                .arg(clip.value(QStringLiteral("height")).toInt())
                .arg(clip.value(QStringLiteral("frames")).toVariant().toLongLong());
        }
        return QStringLiteral("RED SDK 已启用，但读不到这个 R3D：%1\n"
                              "先用 REDCINE-X 确认文件，或导出 ProRes 4444 再渲染。")
            .arg(red.value(QStringLiteral("clipError")).toString());
    }

    // ---- actions ---------------------------------------------------------

    bool requireInput() {
        if (inputEdit_->text().trimmed().isEmpty() || !QFileInfo::exists(inputEdit_->text())) {
            QMessageBox::warning(this, QStringLiteral("缺少素材"),
                                 QStringLiteral("请先选择一个存在的视频文件。"));
            return false;
        }
        if (!QFileInfo::exists(cliPath_)) {
            QMessageBox::critical(
                this, QStringLiteral("找不到 CLI"),
                QStringLiteral("找不到 spektra-roll-cli。\n请先双击 scripts\\build-ui.cmd 重新编译。"));
            return false;
        }
        if (isRedPath(inputEdit_->text().trimmed()) && !redAvailable_) {
            status_->setText(redBlockMessage(inputEdit_->text().trimmed()));
            return false;
        }
        return true;
    }

    bool bakeLut(bool interactive = true) {
        if (spectralEngine()) {
            if (interactive) {
                status_->setText(QStringLiteral("光谱引擎直接计算物理链，不需要 LUT"));
            }
            return false;
        }
        if (!redInput_ && ocioActive() && ocioSpace_->currentData().toString().isEmpty()) {
            const QString message =
                QStringLiteral("OCIO 输入还没选好：先点“读取”，再选一个色彩空间。");
            status_->setText(message);
            if (interactive) QMessageBox::warning(this, QStringLiteral("OpenColorIO"), message);
            return false;
        }
        QString path = lutEdit_->text().trimmed();
        if (path.isEmpty()) path = repoRoot_ + QStringLiteral("/out/ui-portra400.cube");
        QDir().mkpath(QFileInfo(path).absolutePath());

        // D063: an OCIO colour space replaces --input; the decode is baked into
        // the LUT, so `lut apply` and `video` never need the library.
        QStringList args{QStringLiteral("lut"), QStringLiteral("bake")};
        const bool redLut = redInput_ && !spectralEngine();
        if (redLut) {
            // An R3D frame decoded for the LUT is Log3G10/RWG, not the UI's
            // "source" camera log. Force the bundled OCIO config and space.
            QString config = repoRoot_ + QStringLiteral(
                                            "/data/ocio/"
                                            "studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio");
            if (!QFileInfo::exists(config)) config = defaultOcioConfig();
            if (config.isEmpty() || !QFileInfo::exists(config)) {
                const QString message =
                    QStringLiteral("R3D LUT 需要 data/ocio 里的 ACES 配置；找不到 OCIO 配置。");
                status_->setText(message);
                if (interactive) QMessageBox::critical(this, QStringLiteral("R3D LUT"), message);
                return false;
            }
            args << QStringLiteral("--ocio-config") << config << QStringLiteral("--ocio-input")
                 << QStringLiteral("Log3G10 REDWideGamutRGB");
        } else if (ocioActive()) {
            args << ocioArgs();
        } else {
            args << QStringLiteral("--input") << source_->currentData().toString();
        }
        args << QStringLiteral("--film") << filmCombo_->currentData().toString()
             << QStringLiteral("--paper") << paperCombo_->currentData().toString()
             << QStringLiteral("--exposure") << number(exposure_->value(), 2)
             << QStringLiteral("--scan") << scan_->currentData().toString()
             << QStringLiteral("--contrast") << number(contrast_->value(), 2)
             << QStringLiteral("--couplers")
             << (couplers_->isChecked() ? QStringLiteral("on") : QStringLiteral("off"))
             << QStringLiteral("--gamut")
             << (gamut_->isChecked() ? QStringLiteral("oklch") : QStringLiteral("off"))
             << QStringLiteral("--out") << path;
        const QString trc = trc_->currentData().toString();
        if (trc == QStringLiteral("pq") || trc == QStringLiteral("hlg")) {
            args << QStringLiteral("--hdr") << trc << QStringLiteral("--peak")
                 << QStringLiteral("1000");
        }

        QString output;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const bool ok = runSync(cliPath_, args, &output);
        QApplication::restoreOverrideCursor();
        appendLog(output);
        if (!ok) {
            status_->setText(QStringLiteral("LUT 烘焙失败"));
            if (interactive) QMessageBox::critical(this, QStringLiteral("烘焙失败"), output);
            return false;
        }
        lutEdit_->setText(QDir::toNativeSeparators(path));
        lutEdit_->setCursorPosition(0);
        lutBaked_ = true;
        lutStale_ = false;
        QFile params(path + QStringLiteral(".params"));
        if (params.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            params.write(lookSignature().toUtf8());
        }
        status_->setText(
            redLut ? QStringLiteral("LUT 已烘焙（R3D Log3G10/RWG）：") + QFileInfo(path).fileName()
                   : (ocioActive()
                          ? QStringLiteral("LUT 已烘焙（OCIO %1）：%2")
                                .arg(ocioSpace_->currentText(), QFileInfo(path).fileName())
                          : QStringLiteral("LUT 已烘焙：") + QFileInfo(path).fileName()));
        if (trc != QStringLiteral("srgb")) {
            appendLog(QStringLiteral("提示：HDR LUT 的画面在普通 SDR 屏上会偏暗，最终请用 HDR 显示器/iPhone 看。\n"));
        }
        return true;
    }

    bool ensureLut() {
        if (spectralEngine()) return true;
        const QString path = lutEdit_->text().trimmed();
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            if (lutBaked_ && lutStale_) return bakeLut();
            return true;
        }
        const auto answer = QMessageBox::question(
            this, QStringLiteral("没有 LUT"),
            QStringLiteral("还没有可用的 LUT，是否现在按当前参数烘焙一张？"));
        return answer == QMessageBox::Yes && bakeLut();
    }

    void markLookChanged() {
        if (!lutBaked_) return;
        lutStale_ = true;
        status_->setText(QStringLiteral("曝光/扫描/反差已改：预览或渲染时会重新烘焙 LUT"));
    }

    QString lookSignature() const {
        return QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12")
            .arg(engine_->currentData().toString())
            .arg(source_->currentData().toString())
            .arg(number(exposure_->value(), 4))
            .arg(scan_->currentData().toString())
            .arg(number(contrast_->value(), 4))
            .arg(trc_->currentData().toString())
            .arg(couplers_->isChecked() ? QStringLiteral("dir") : QStringLiteral("-"))
            .arg(gamut_->isChecked() ? QStringLiteral("oklch") : QStringLiteral("-"))
            .arg(QFileInfo(filmCombo_->currentData().toString()).fileName())
            .arg(QFileInfo(paperCombo_->currentData().toString()).fileName())
            .arg(ocioActive() ? QFileInfo(ocioConfig_->text().trimmed()).fileName() +
                                    QLatin1Char(':') + ocioSpace_->currentData().toString()
                              : QStringLiteral("-"))
            .arg(redInput_ ? QStringLiteral("r3d:") + redScale_->currentData().toString()
                           : QStringLiteral("-"));
    }

    QStringList textureArgs() const {
        QStringList args;
        if (textureGpu_ != nullptr && textureGpu_->isChecked()) {
            args << QStringLiteral("--texture-engine") << QStringLiteral("gpu");
        }
        if (grain_->value() > 0.0) {
            args << QStringLiteral("--grain") << number(grain_->value())
                 << QStringLiteral("--grain-rate") << QString::number(grainRate_->value());
        }
        if (halation_->value() > 0.0) {
            args << QStringLiteral("--halation") << number(halation_->value());
        }
        if (glare_->value() > 0.0) {
            args << QStringLiteral("--glare") << number(glare_->value());
        }
        if (diffusion_->value() > 0.0) {
            args << QStringLiteral("--diffusion") << number(diffusion_->value());
        }
        return args;
    }

    void probeInput() {
        const QString path = inputEdit_->text().trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            clipInfo_ = ClipInfo{};
            lastProbePath_.clear();
            stopPreviewServer();
            clipNameLabel_->setText(QStringLiteral("未选择素材"));
            clipNameLabel_->setToolTip(QString());
            clipInfoLabel_->setText(QStringLiteral("未选择素材（可以把视频拖进窗口）"));
            timeline_->setRange(0, 0);
            timeLabel_->setText(QStringLiteral("--"));
            return;
        }
        // D071: `.r3d` uses the CLI's optional RED plugin. ffprobe still gives
        // width/fps/frames/timecode; the plugin decides whether we can decode.
        if (isRedPath(path)) {
            redInput_ = true;
            // half/quarter R3D heights can be odd; ProRes handles them while
            // H.264/HEVC 4:2:0 does not. Keep the combo editable for full-res.
            if (codec_ != nullptr) selectData(codec_, QStringLiteral("prores"));
            stopPreviewServer();
            const QJsonObject red = queryRedInfo(path);
            redAvailable_ = red.value(QStringLiteral("available")).toBool();
            ClipInfo info = probeClip(path);
            if (!info.valid()) {
                const QJsonObject clip = red.value(QStringLiteral("clip")).toObject();
                if (!clip.isEmpty()) {
                    info.width = clip.value(QStringLiteral("width")).toInt();
                    info.height = clip.value(QStringLiteral("height")).toInt();
                    info.fps = clip.value(QStringLiteral("fps")).toDouble();
                    info.frames = clip.value(QStringLiteral("frames")).toVariant().toLongLong();
                    info.timecode = clip.value(QStringLiteral("timecode")).toString();
                    info.duration = info.fps > 0.0 ? info.frames / info.fps : 0.0;
                }
            }
            clipNameLabel_->setText(QFileInfo(path).fileName());
            clipNameLabel_->setToolTip(path);
            if (!info.valid()) {
                clipInfo_ = ClipInfo{};
                timeline_->setRange(0, 0);
                timeLabel_->setText(QStringLiteral("--"));
                const QString label = QStringLiteral("R3D：读不到元数据（检查 ffprobe / RED 插件）");
                clipInfoLabel_->setText(label);
                status_->setText(label);
                return;
            }
            clipInfo_ = info;
            if (path != lastProbePath_) {
                lastProbePath_ = path;
                preview_->setImage(QImage());
                gradedImage_ = QImage();
                originalImage_ = QImage();
                originalTime_ = -1.0;
            }
            if (info.frames > 1) timeline_->setRange(0, static_cast<int>(info.frames - 1));
            previewTime_->setMaximum(qMax(1.0, info.duration));
            if (previewTime_->value() > info.duration) {
                previewTime_->setValue(qMax(0.0, info.duration - 0.001));
            }
            syncTimelineFromTime();
            QString label = QStringLiteral("R3D %1×%2 · %3 fps · %4 帧")
                                .arg(info.width)
                                .arg(info.height)
                                .arg(number(info.fps, 3))
                                .arg(info.frames);
            if (!info.timecode.isEmpty()) label += QStringLiteral(" · TC ") + info.timecode;
            label += redAvailable_
                         ? QStringLiteral(" · RED SDK 已启用；预览/渲染走 build-red")
                         : QStringLiteral(" · 没有 RED SDK：先 scripts\\build-red.cmd 本地编译");
            clipInfoLabel_->setText(label);
            status_->setText(label);
            appendLog(QStringLiteral("R3D: ") + QDir::toNativeSeparators(path) + QLatin1Char('\n'));
            if (!redAvailable_) {
                const QString message = red.value(QStringLiteral("message")).toString();
                if (!message.isEmpty()) {
                    appendLog(QStringLiteral("RED: ") + message + QLatin1Char('\n'));
                }
            }
            return;
        }
        redInput_ = false;
        redAvailable_ = false;
        const ClipInfo info = probeClip(path);
        if (!info.valid()) {
            clipInfo_ = ClipInfo{};
            clipInfoLabel_->setText(QStringLiteral("读不到素材信息，检查 ffmpeg/ffprobe 路径"));
            timeline_->setRange(0, 0);
            timeLabel_->setText(QStringLiteral("--"));
            return;
        }
        clipInfo_ = info;
        clipNameLabel_->setText(QFileInfo(path).fileName());
        clipNameLabel_->setToolTip(path);
        if (path != lastProbePath_) {
            lastProbePath_ = path;
            preview_->setImage(QImage());
            gradedImage_ = QImage();
            originalImage_ = QImage();
            originalTime_ = -1.0;
        }
        QString label = QStringLiteral("%1 × %2 · %3 fps · %4 s · %5 帧")
                            .arg(info.width)
                            .arg(info.height)
                            .arg(number(info.fps, 3))
                            .arg(number(info.duration, 2))
                            .arg(info.frames);
        if (!info.timecode.isEmpty()) label += QStringLiteral(" · TC ") + info.timecode;
        clipInfoLabel_->setText(label);
        if (info.frames > 1) timeline_->setRange(0, static_cast<int>(info.frames - 1));
        previewTime_->setMaximum(qMax(1.0, info.duration));
        if (previewTime_->value() > info.duration) {
            previewTime_->setValue(qMax(0.0, info.duration - 0.001));
        }
        syncTimelineFromTime();
    }

    void syncTimelineFromTime() {
        updateTimeLabel();
        if (clipInfo_.fps <= 0.0) return;
        const int frame =
            qBound(0, static_cast<int>(std::llround(previewTime_->value() * clipInfo_.fps)),
                   timeline_->maximum());
        if (timeline_->value() != frame) {
            const QSignalBlocker blocker(timeline_);
            timeline_->setValue(frame);
        }
    }

    void updateTimeLabel() {
        if (clipInfo_.fps <= 0.0) {
            timeLabel_->setText(QStringLiteral("--"));
            return;
        }
        const long frame =
            static_cast<long>(std::llround(previewTime_->value() * clipInfo_.fps));
        timeLabel_->setText(QStringLiteral("%1 / %2 · 帧 %3/%4")
                                .arg(formatClock(previewTime_->value()),
                                     formatClock(clipInfo_.duration))
                                .arg(frame)
                                .arg(clipInfo_.frames));
    }

    void openOutputFolder() {
        const QString output = outputEdit_->text().trimmed();
        if (output.isEmpty()) return;
        const QFileInfo info(output);
        QDir().mkpath(info.absolutePath());
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
    }

    void showOriginal() {
        if (!requireInput()) return;
        const double time = previewTime_->value();
        if (!originalImage_.isNull() &&
            std::abs(originalTime_ - time) < 0.0005) {
            preview_->setImage(originalImage_);
            status_->setText(QStringLiteral("原片（未套胶片）"));
            return;
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString log;
        QImage image;
        if (!previewFromServer(previewFrameIndex(), false, &image, &log) &&
            !previewLegacy(false, &image, &log)) {
            QApplication::restoreOverrideCursor();
            appendLog(log);
            status_->setText(QStringLiteral("原片预览失败，日志里有原因"));
            return;
        }
        QApplication::restoreOverrideCursor();

        originalImage_ = image;
        originalTime_ = time;
        preview_->setImage(image);
        status_->setText(QStringLiteral("原片（未套胶片）"));
    }

    void restoreGraded() {
        if (gradedImage_.isNull()) {
            previewFrame();
            return;
        }
        preview_->setImage(gradedImage_);
        status_->setText(QStringLiteral("成片预览"));
    }

    // ---- preview server (D048) -------------------------------------------
    //
    // Spawning ffmpeg plus the CLI per preview frame cost 1.7-2.2 s (PNG round
    // trip, LUT parse, D3D setup). One long-lived process decodes the clip into
    // a bounded cache once, then a frame costs colour + texture only.

    static QString quoted(const QString& value) {
        if (!value.contains(QLatin1Char(' '))) return value;
        return QLatin1Char('"') + value + QLatin1Char('"');
    }

    QString previewLookKey() const {
        QString key = engineName() + QLatin1Char('|') + lutEdit_->text();
        const QFileInfo lut(lutEdit_->text());
        if (lut.exists()) {
            key += QLatin1Char('|') + QString::number(lut.lastModified().toMSecsSinceEpoch());
        }
        if (spectralEngine()) key += QLatin1Char('|') + lookArgs(true).join(QLatin1Char(' '));
        else key += QLatin1Char('|') + ocioArgs().join(QLatin1Char(' '));
        return key;
    }

    // Keep in step with the CLI defaults: the server takes all eleven values,
    // then the spread model (1 = physical, D054) and the negative width.
    QString previewTextureLine() const {
        return QStringLiteral(
                   "texture %1 1.4 1 %2 %3 0.006 0.72 %4 0.02 %5 0.004 1 36")
            .arg(number(grain_->value()))
            .arg(grainRate_->value())
            .arg(number(halation_->value()))
            .arg(number(glare_->value()))
            .arg(number(diffusion_->value()));
    }

    int previewFrameIndex() const {
        if (clipInfo_.fps <= 0.0) return 0;
        const int frame = static_cast<int>(std::llround(previewTime_->value() * clipInfo_.fps));
        const int last = clipInfo_.frames > 0 ? static_cast<int>(clipInfo_.frames) - 1 : 0;
        return qBound(0, frame, last);
    }

    void stopPreviewServer() {
        if (previewServer_ == nullptr) return;
        previewServer_->disconnect(this);
        if (previewServer_->state() != QProcess::NotRunning) {
            previewServer_->write("quit\n");
            previewServer_->waitForBytesWritten(200);
            if (!previewServer_->waitForFinished(1000)) {
                previewServer_->kill();
                previewServer_->waitForFinished(1000);
            }
        }
        previewServer_->deleteLater();
        previewServer_ = nullptr;
        previewMatrix_.clear();
        previewLook_.clear();
        previewClip_.clear();
        previewTexture_.clear();
        previewBackend_.clear();
        previewWidth_ = 0;
        previewHeight_ = 0;
        previewFrames_ = 0;
    }

    bool sendPreview(const QString& line, QString* reply, int timeoutMs) {
        if (previewServer_ == nullptr || previewServer_->state() != QProcess::Running) {
            return false;
        }
        previewServer_->write(line.toUtf8() + '\n');
        if (!previewServer_->waitForBytesWritten(2000)) return false;
        QByteArray buffer;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            if (previewServer_->waitForReadyRead(100)) {
                buffer += previewServer_->readAllStandardOutput();
                const int newline = buffer.indexOf('\n');
                if (newline >= 0) {
                    if (reply != nullptr) {
                        *reply = QString::fromUtf8(buffer.left(newline)).trimmed();
                    }
                    return true;
                }
            }
            if (previewServer_->state() != QProcess::Running) return false;
        }
        return false;
    }

    bool ensurePreviewServer(QString* log) {
        const QString clip = inputEdit_->text().trimmed();
        if (clip.isEmpty() || !QFileInfo::exists(clip)) return false;
        const bool debug = !qEnvironmentVariableIsEmpty("SPEKTRA_PREVIEW_DEBUG");
        QElapsedTimer debugTimer;
        debugTimer.start();
        const auto mark = [&](const char* what) {
            if (!debug) return;
            std::fprintf(stderr, "preview %-10s %5lld ms\n", what,
                         static_cast<long long>(debugTimer.elapsed()));
            std::fflush(stderr);
        };

        // A different clip can carry a different container matrix, and the
        // matrix is a startup option of the server: restart it (D061).
        if (previewServer_ != nullptr && previewMatrix_ != decodeMatrix()) stopPreviewServer();

        if (previewServer_ == nullptr) {
            previewServer_ = new QProcess(this);
            previewServer_->setProcessChannelMode(QProcess::SeparateChannels);
            previewServer_->setProgram(cliPath_);
            previewServer_->setWorkingDirectory(repoRoot_);
            const QString matrix = decodeMatrix();
            previewMatrix_ = matrix;
            previewServer_->setArguments(
                QStringList{QStringLiteral("preview"), QStringLiteral("--ffmpeg"),
                            ffmpegEdit_->text(), QStringLiteral("--in-matrix"), matrix});
            previewServer_->start();
            if (!previewServer_->waitForStarted(4000)) {
                if (log != nullptr) *log += QString::fromUtf8(previewServer_->readAllStandardError());
                stopPreviewServer();
                return false;
            }
        }
        if (previewServer_->state() != QProcess::Running) {
            stopPreviewServer();
            return false;
        }

        const QString lookKey = previewLookKey();
        if (lookKey != previewLook_) {
            QStringList args{QStringLiteral("look")};
            if (spectralEngine()) {
                args << QStringLiteral("--engine") << engineName() << lookArgs(true);
            } else {
                args << QStringLiteral("--lut") << lutEdit_->text();
                // D063: the LUT carries the decode, but `raw` still needs it.
                args << ocioArgs();
            }
            QStringList safe;
            for (const QString& arg : args) safe << quoted(arg);
            QString reply;
            if (!sendPreview(safe.join(QLatin1Char(' ')), &reply, 20000) ||
                !reply.startsWith(QStringLiteral("ok"))) {
                if (log != nullptr) *log += QString::fromUtf8(previewServer_->readAllStandardError());
                stopPreviewServer();
                return false;
            }
            previewLook_ = lookKey;
            previewTexture_.clear();
            mark("look");
        }

        if (previewClip_ != clip) {
            QString reply;
            if (!sendPreview(QStringLiteral("open ") + quoted(clip), &reply, 600000) ||
                !reply.startsWith(QStringLiteral("ok open"))) {
                if (log != nullptr) *log += QString::fromUtf8(previewServer_->readAllStandardError());
                stopPreviewServer();
                return false;
            }
            const QStringList parts = reply.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                previewWidth_ = parts[2].toInt();
                previewHeight_ = parts[3].toInt();
                // parts[4] is the RAM cache depth, parts[5] the clip length; the
                // server can serve every frame (RAM or proxy, D053).
                previewFrames_ = parts[5].toInt();
            }
            previewClip_ = clip;
            mark("open");
            if (debug) {
                const QByteArray serverLog = previewServer_->readAllStandardError();
                if (!serverLog.isEmpty()) std::fprintf(stderr, "%s", serverLog.constData());
            }
        }
        return previewFrames_ > 0;
    }

    bool loadPreviewRgba(const QString& path, int width, int height, QImage* image,
                         QString* log) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QByteArray data = file.readAll();
        file.close();
        const qsizetype expected = static_cast<qsizetype>(width) * height * 4;
        if (data.size() != expected) {
            if (log != nullptr) {
                *log += QStringLiteral("preview frame is %1 bytes, expected %2\n")
                            .arg(data.size())
                            .arg(expected);
            }
            return false;
        }
        const QImage view(reinterpret_cast<const uchar*>(data.constData()), width, height,
                          width * 4, QImage::Format_RGBA8888);
        *image = view.copy();
        return !image->isNull();
    }

    bool previewFromServer(int frame, bool graded, QImage* image, QString* log) {
        if (redInput_) return false;  // R3D uses the red-decode path below (D071)
        if (!ensurePreviewServer(log)) return false;
        if (frame < 0 || frame >= previewFrames_) return false;

        if (graded) {
            const QString backend = textureGpu_ != nullptr && textureGpu_->isChecked()
                                        ? QStringLiteral("gpu")
                                        : QStringLiteral("cpu");
            if (backend != previewBackend_) {
                QString reply;
                if (!sendPreview(QStringLiteral("backend ") + backend, &reply, 5000) ||
                    !reply.startsWith(QStringLiteral("ok"))) {
                    return false;
                }
                previewBackend_ = backend;
            }
            const QString textureLine = previewTextureLine();
            if (textureLine != previewTexture_) {
                QString reply;
                if (!sendPreview(textureLine, &reply, 5000) ||
                    !reply.startsWith(QStringLiteral("ok"))) {
                    return false;
                }
                previewTexture_ = textureLine;
            }
        }

        const QString raw = repoRoot_ + QStringLiteral("/out/ui-preview.rgba");
        QFile::remove(raw);
        const QString command =
            QStringLiteral("%1 %2 %3")
                .arg(graded ? QStringLiteral("frame") : QStringLiteral("raw"))
                .arg(frame)
                .arg(quoted(raw));
        const bool debug = !qEnvironmentVariableIsEmpty("SPEKTRA_PREVIEW_DEBUG");
        QElapsedTimer timer;
        timer.start();
        QString reply;
        if (!sendPreview(command, &reply, 60000) || !reply.startsWith(QStringLiteral("ok"))) {
            return false;
        }
        int width = previewWidth_;
        int height = previewHeight_;
        // Reply: "ok frame <n> <w> <h>". Frames can come from the RAM cache or
        // from the proxy, which may have different sizes (D053).
        const QStringList parts = reply.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() >= 5) {
            width = parts[3].toInt();
            height = parts[4].toInt();
        }
        if (debug) {
            std::fprintf(stderr, "preview frame    %5lld ms\n",
                         static_cast<long long>(timer.elapsed()));
        }
        const bool loaded = loadPreviewRgba(raw, width, height, image, log);
        if (debug) {
            std::fprintf(stderr, "preview load     %5lld ms\n",
                         static_cast<long long>(timer.elapsed()));
        }
        return loaded;
    }

    // The pre-D048 path: one ffmpeg extraction plus one CLI run. Still used when
    // the server cannot start or the frame is outside the decode cache.
    // D071: R3D preview. The preview server does not read `.r3d`; decode one
    // frame through the same RedReader plugin and then run `lut apply` (or the
    // spectral engine) on it. Scrubbing is slower than the 960 px proxy, but
    // this is the same colour path as the full render.
    bool previewRedLegacy(bool graded, QImage* image, QString* log) {
        QDir().mkpath(repoRoot_ + QStringLiteral("/out"));
        const QString source =
            repoRoot_ + QStringLiteral("/out/ui-red-src.png");
        const QString output =
            repoRoot_ + QStringLiteral("/out/ui-red.png");
        const int frame = previewFrameIndex();
        const QString redFormat =
            (graded && spectralEngine()) ? QStringLiteral("srgb") : QStringLiteral("log3g10");
        QStringList decodeArgs{
            QStringLiteral("red-decode"),
            QStringLiteral("--in"), inputEdit_->text(),
            QStringLiteral("--frame"), QString::number(frame),
            QStringLiteral("--scale"), redScale_->currentData().toString(),
            QStringLiteral("--format"), graded ? redFormat : QStringLiteral("srgb"),
            QStringLiteral("--out"), graded ? source : output};
        QString outputText;
        if (!runSync(cliPath_, decodeArgs, &outputText)) {
            *log += outputText;
            return false;
        }
        if (graded) {
            QStringList args{QStringLiteral("lut"), QStringLiteral("apply")};
            if (spectralEngine()) {
                args << QStringLiteral("--engine") << engineName();
                args << lookArgs(true);
            } else {
                args << QStringLiteral("--lut") << lutEdit_->text();
            }
            args << QStringLiteral("--in") << source << QStringLiteral("--out") << output;
            args << textureArgs();
            if (!runSync(cliPath_, args, &outputText)) {
                *log += outputText;
                return false;
            }
        }
        const QImage loaded(output);
        if (loaded.isNull()) return false;
        *image = loaded;
        return true;
    }

    bool previewLegacy(bool graded, QImage* image, QString* log) {
        if (redInput_) return previewRedLegacy(graded, image, log);
        QDir().mkpath(repoRoot_ + QStringLiteral("/out"));
        const QString suffix = graded ? QStringLiteral("preview") : QStringLiteral("original");
        const QString source =
            repoRoot_ + QStringLiteral("/out/ui-") + suffix + QStringLiteral("-src.png");
        const QString output =
            repoRoot_ + QStringLiteral("/out/ui-") + suffix + QStringLiteral(".png");
        const QString matrix = decodeMatrix();
        QStringList ffmpegArgs{
            QStringLiteral("-v"),           QStringLiteral("error"),
            QStringLiteral("-y"),           QStringLiteral("-ss"),
            number(previewTime_->value()),  QStringLiteral("-i"),
            inputEdit_->text(),             QStringLiteral("-frames:v"),
            QStringLiteral("1"),            QStringLiteral("-vf"),
            QStringLiteral("scale=in_color_matrix=") + matrix +
                QStringLiteral(":in_range=auto:out_range=full"),
            QStringLiteral("-pix_fmt"),     QStringLiteral("rgb48le"),
            source};

        QString outputText;
        if (!runSync(ffmpegEdit_->text(), ffmpegArgs, &outputText)) {
            *log += outputText;
            return false;
        }
        QStringList args;
        if (graded) {
            args << QStringLiteral("lut") << QStringLiteral("apply");
            if (spectralEngine()) {
                args << QStringLiteral("--engine") << engineName();
                args << lookArgs(true);
            } else {
                args << QStringLiteral("--lut") << lutEdit_->text();
            }
            args << QStringLiteral("--in") << source << QStringLiteral("--out") << output;
            args << textureArgs();
        } else {
            args << QStringLiteral("convert") << QStringLiteral("--in") << source
                 << QStringLiteral("--out") << output;
            if (ocioActive()) {
                args << ocioArgs();
            } else {
                const QString from = source_->currentData().toString();
                if (from != QStringLiteral("srgb")) {
                    args << QStringLiteral("--from") << from;
                }
            }
        }
        if (!runSync(cliPath_, args, &outputText)) {
            *log += outputText;
            return false;
        }
        const QImage loaded(output);
        if (loaded.isNull()) return false;
        *image = loaded;
        return true;
    }

    void previewFrame() {
        if (!requireInput() || !ensureLut()) return;
        const int frame = previewFrameIndex();
        // R3D preview decodes a full frame through the SDK; only render on
        // release, not on every scrub tick.
        if (redInput_ && scrubbing_) return;
        // While scrubbing, never fall back to the slow single-frame path.
        if (scrubbing_ && previewFrames_ > 0 && frame >= previewFrames_) return;

        if (!scrubbing_) {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            status_->setText(spectralEngine()
                                 ? (engineName() == QStringLiteral("spectral-gpu")
                                        ? QStringLiteral("光谱 GPU 预览中（D3D11 逐像素物理链）…")
                                        : QStringLiteral("光谱预览中（逐像素物理链）…"))
                                 : QStringLiteral("LUT 预览中…"));
        }
        QString log;
        QImage image;
        QElapsedTimer timer;
        timer.start();
        bool fromServer = previewFromServer(frame, true, &image, &log);
        if (!fromServer) {
            fromServer = false;
            if (!previewLegacy(true, &image, &log)) {
                if (!scrubbing_) QApplication::restoreOverrideCursor();
                appendLog(log);
                status_->setText(QStringLiteral("预览失败，日志里有原因"));
                return;
            }
        }
        if (!scrubbing_) QApplication::restoreOverrideCursor();

        preview_->setImage(image);
        gradedImage_ = image;
        originalImage_ = QImage();
        originalTime_ = -1.0;
        const bool behindCache =
            previewFrames_ > 0 && previewFrameIndex() >= previewFrames_;
        status_->setText(QStringLiteral("预览：%1 × %2 @ %3 s · %4 ms%5")
                             .arg(image.width())
                             .arg(image.height())
                             .arg(number(previewTime_->value(), 2))
                             .arg(timer.elapsed())
                             .arg(fromServer ? QStringLiteral(" · 整段缓存")
                                             : (behindCache ? QStringLiteral(" · 超出缓存，单帧")
                                                            : QStringLiteral(" · 单帧"))));
    }

    bool spectralEngine() const {
        if (engine_ == nullptr) return false;
        const QString data = engine_->currentData().toString();
        return data == QStringLiteral("spectral") || data == QStringLiteral("spectral-gpu");
    }

    QString engineName() const {
        return engine_ != nullptr ? engine_->currentData().toString() : QStringLiteral("lut");
    }

    // ---- OpenColorIO (D063) ----------------------------------------------

    bool ocioActive() const {
        return ocioEnabled_ != nullptr && ocioEnabled_->isChecked();
    }

    QString defaultOcioConfig() const {
        const QString env = qEnvironmentVariable("OCIO");
        if (!env.isEmpty()) return QDir::toNativeSeparators(env);
        const QString bundled = repoRoot_ + QStringLiteral(
                                                "/data/ocio/"
                                                "studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio");
        return QFileInfo::exists(bundled) ? bundled : QString();
    }

    // `--ocio-config` / `--ocio-input`, or empty when the built-in input is used.
    QStringList ocioArgs() const {
        QStringList args;
        if (!ocioActive()) return args;
        const QString config = ocioConfig_ != nullptr ? ocioConfig_->text().trimmed() : QString();
        if (!config.isEmpty()) {
            args << QStringLiteral("--ocio-config") << QDir::toNativeSeparators(config);
        }
        const QString space =
            ocioSpace_ != nullptr ? ocioSpace_->currentData().toString() : QString();
        if (!space.isEmpty()) args << QStringLiteral("--ocio-input") << space;
        return args;
    }

    void updateOcioUi() {
        const bool on = ocioActive();
        if (source_ != nullptr) source_->setEnabled(!on);
        if (ocioConfig_ != nullptr) ocioConfig_->setEnabled(on);
        if (ocioSpace_ != nullptr) ocioSpace_->setEnabled(on);
        if (ocioRefreshButton_ != nullptr) ocioRefreshButton_->setEnabled(on);
        if (ocioInfoLabel_ != nullptr && !on) {
            ocioInfoLabel_->setText(
                QStringLiteral("勾选后点“读取”，从配置里列出所有色彩空间。"));
        }
        if (engine_ != nullptr) updateEngineUi();
    }

    // Fills the colour-space list from `ocio-info --json`. The CLI owns OCIO;
    // the UI only parses names so it never has to link the library.
    void refreshOcioSpaces(bool announce) {
        if (ocioSpace_ == nullptr || ocioConfig_ == nullptr) return;
        QStringList args{QStringLiteral("ocio-info"), QStringLiteral("--json")};
        const QString config = ocioConfig_->text().trimmed();
        if (!config.isEmpty()) {
            args << QStringLiteral("--ocio-config") << QDir::toNativeSeparators(config);
        }
        QString output;
        QString errors;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const bool ok = runCaptureSync(cliPath_, args, &output, &errors);
        QApplication::restoreOverrideCursor();
        if (!ok) {
            ocioSpace_->clear();
            const QString message =
                errors.trimmed().isEmpty() ? output.trimmed() : errors.trimmed();
            ocioInfoLabel_->setText(QStringLiteral("读不到 OCIO：") + message);
            if (announce) {
                QMessageBox::warning(this, QStringLiteral("OpenColorIO"), message);
            }
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(output.toUtf8()).object();
        const QJsonArray spaces = root.value(QStringLiteral("colorSpaces")).toArray();
        if (spaces.isEmpty()) {
            ocioSpace_->clear();
            ocioInfoLabel_->setText(QStringLiteral("这个配置里没有色彩空间"));
            return;
        }
        const QString previous = ocioSpace_->currentData().toString();
        {
            const QSignalBlocker blocker(ocioSpace_);
            ocioSpace_->clear();
            for (const QJsonValue& value : spaces) {
                const QString name = value.toString();
                if (!name.isEmpty()) ocioSpace_->addItem(name, name);
            }
            int index = ocioSpace_->findData(previous);
            if (index < 0) index = ocioSpace_->findData(QStringLiteral("S-Log3 S-Gamut3.Cine"));
            if (index < 0) index = 0;
            ocioSpace_->setCurrentIndex(index);
        }
        ocioInfoLabel_->setText(
            QStringLiteral("OCIO %1 · %2 个色彩空间 · 线性空间 %3")
                .arg(root.value(QStringLiteral("version")).toString())
                .arg(spaces.size())
                .arg(root.value(QStringLiteral("linearSpace")).toString()));
        if (announce) {
            appendLog(QStringLiteral("OCIO: ") + ocioInfoLabel_->text() + QLatin1Char('\n'));
        }
        markLookChanged();
    }

    // Everything the physical chain needs at run time, shared by the spectral
    // image and video paths. `withHdr` is false for video because `--trc`
    // already selects PQ/HLG there.
    QStringList lookArgs(bool withHdr) const {
        QStringList args;
        // R3D spectral frames are decoded to sRGB Rec.709 by the plugin; the
        // UI's "source" camera log and OCIO decode do not apply.
        if (redInput_) {
            // intentionally no --input / --ocio-*
        } else if (ocioActive()) {
            args << ocioArgs();
        } else {
            args << QStringLiteral("--input") << source_->currentData().toString();
        }
        args << QStringLiteral("--film") << filmCombo_->currentData().toString()
             << QStringLiteral("--paper") << paperCombo_->currentData().toString()
             << QStringLiteral("--exposure") << number(exposure_->value(), 2)
             << QStringLiteral("--scan") << scan_->currentData().toString()
             << QStringLiteral("--contrast") << number(contrast_->value(), 2)
             << QStringLiteral("--couplers")
             << (couplers_->isChecked() ? QStringLiteral("on") : QStringLiteral("off"))
             << QStringLiteral("--gamut")
             << (gamut_->isChecked() ? QStringLiteral("oklch") : QStringLiteral("off"));
        const QString trc = trc_->currentData().toString();
        if (withHdr && (trc == QStringLiteral("pq") || trc == QStringLiteral("hlg"))) {
            args << QStringLiteral("--hdr") << trc << QStringLiteral("--peak")
                 << QStringLiteral("1000");
        }
        return args;
    }

    void updateEngineUi() {
        const bool spectral = spectralEngine();
        if (lutEdit_ != nullptr) lutEdit_->setEnabled(!spectral);
        if (lutBrowseButton_ != nullptr) lutBrowseButton_->setEnabled(!spectral);
        if (bakeButton_ != nullptr) bakeButton_->setEnabled(!spectral);
        if (status_ != nullptr) {
            const QString data = engineName();
            status_->setText(
                data == QStringLiteral("spectral-gpu")
                    ? (ocioActive()
                           ? QStringLiteral("OCIO 输入 + 光谱 GPU：GPU 链不支持 OCIO，会自动用 CPU 光谱")
                           : QStringLiteral("光谱 GPU 引擎：D3D11 上跑逐像素物理链，"
                                            "无显卡时自动退回 CPU"))
                    : (spectral
                           ? QStringLiteral("光谱引擎：逐像素物理链，约 1.2 s / 2.6 MP 每帧")
                           : (ocioActive()
                                  ? QStringLiteral("LUT 引擎：每帧查表；OCIO 解码已烘进 LUT")
                                  : QStringLiteral("LUT 引擎：每帧查表"))));
        }
    }

    QStringList buildVideoArgs(const QString& input, const QString& output,
                               const QString& lut) const {
        QStringList args{QStringLiteral("video")};
        if (spectralEngine()) {
            args << QStringLiteral("--engine") << engineName();
            args << lookArgs(false);
        } else {
            args << QStringLiteral("--lut") << lut;
        }
        args << QStringLiteral("--in") << input << QStringLiteral("--out") << output
             << QStringLiteral("--trc") << trc_->currentData().toString()
             << QStringLiteral("--ffmpeg") << ffmpegEdit_->text();
        if (redInput_) {
            args << QStringLiteral("--red-scale") << redScale_->currentData().toString();
        }
        args << textureArgs();
        args << QStringLiteral("--codec") << codec_->currentData().toString();
        if (start_->value() > 0.0) {
            args << QStringLiteral("--start") << number(start_->value());
        }
        if (frames_->value() > 0) {
            args << QStringLiteral("--frames") << QString::number(frames_->value());
        }
        if (segment_->value() > 0) {
            args << QStringLiteral("--segment") << QString::number(segment_->value());
            if (resume_->isChecked()) args << QStringLiteral("--resume");
        }
        return args;
    }

    void startRender() {
        if (!requireInput() || !ensureLut() || rendering_) return;
        if (queueRunning_) {
            QMessageBox::information(this, QStringLiteral("队列正在渲染"),
                                     QStringLiteral("先停止队列，或把这一条“加入队列”。"));
            return;
        }

        const QString output = outputEdit_->text().trimmed();
        if (output.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("缺少输出"),
                                 QStringLiteral("请指定输出文件。"));
            return;
        }
        QDir().mkpath(QFileInfo(output).absolutePath());

        const QStringList args = buildVideoArgs(inputEdit_->text(), output, lutEdit_->text());

        log_->clear();
        appendLog(QStringLiteral("> ") + cliPath_ + QLatin1Char(' ') + args.join(QLatin1Char(' ')) +
                  QLatin1Char('\n'));
        pending_.clear();
        canceled_ = false;
        rendering_ = true;
        setBusy(true);
        progress_->setRange(0, 0);
        status_->setText(spectralEngine()
                             ? QStringLiteral("光谱渲染中：逐像素物理链，约 1.2 秒/帧")
                             : QStringLiteral("渲染中…"));

        auto* process = new QProcess(this);
        renderProcess_ = process;
        process->setWorkingDirectory(repoRoot_);
        process->setProcessChannelMode(QProcess::MergedChannels);
        connect(process, &QProcess::readyRead, this, [this] { drainRenderOutput(); });
        connect(process, &QProcess::finished, this,
                [this, process](int code, QProcess::ExitStatus exitStatus) {
                    drainRenderOutput();
                    const bool ok = !canceled_ && exitStatus == QProcess::NormalExit && code == 0;
                    rendering_ = false;
                    renderProcess_ = nullptr;
                    process->deleteLater();
                    setBusy(false);
                    if (canceled_) {
                        status_->setText(QStringLiteral("已取消；完成的段已保留，下次可续渲"));
                        progress_->setRange(0, 100);
                    } else if (ok) {
                        status_->setText(QStringLiteral("渲染完成：") + outputEdit_->text());
                        progress_->setRange(0, 100);
                        progress_->setValue(100);
                        if (!suppressCompletionDialog_) {
                            const auto answer = QMessageBox::question(
                                this, QStringLiteral("完成"),
                                QStringLiteral("渲染完成。现在打开文件所在文件夹吗？"));
                            if (answer == QMessageBox::Yes) {
                                QDesktopServices::openUrl(QUrl::fromLocalFile(
                                    QFileInfo(outputEdit_->text()).absolutePath()));
                            }
                        }
                    } else {
                        status_->setText(QStringLiteral("渲染失败，日志里有原因"));
                        progress_->setRange(0, 100);
                    }
                });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
            if (!canceled_ && !queueCanceled_) {
                appendLog(QStringLiteral("进程错误：") + process->errorString() +
                          QLatin1Char('\n'));
            }
        });
        process->start(cliPath_, args);
        if (!process->waitForStarted(5000)) {
            appendLog(QStringLiteral("无法启动 CLI：") + process->errorString() + QLatin1Char('\n'));
            rendering_ = false;
            renderProcess_ = nullptr;
            process->deleteLater();
            setBusy(false);
            status_->setText(QStringLiteral("无法启动渲染"));
        }
    }

    void cancelRender() {
        if (!rendering_ || renderProcess_ == nullptr) return;
        canceled_ = true;
        status_->setText(QStringLiteral("正在取消…"));
        renderProcess_->kill();
    }

    void drainRenderOutput() {
        if (renderProcess_ == nullptr) return;
        const QString text = QString::fromLocal8Bit(renderProcess_->readAll());
        if (text.isEmpty()) return;
        appendLog(text);
        pending_ += text;
        const QRegularExpression framesRe(QStringLiteral("(\\d+)\\s*/\\s*(\\d+)\\s+frames"));
        int newline = 0;
        while ((newline = pending_.indexOf(QLatin1Char('\n'))) >= 0) {
            const QString line = pending_.left(newline).trimmed();
            pending_.remove(0, newline + 1);
            if (line.isEmpty()) continue;
            status_->setText(statusLine(line));
            const auto match = framesRe.match(line);
            if (match.hasMatch()) {
                const int done = match.captured(1).toInt();
                const int total = match.captured(2).toInt();
                progress_->setRange(0, total);
                progress_->setValue(done);
            }
        }
    }

    // ---- render queue ----------------------------------------------------

    QString uniqueOutputPath(const QString& base) const {
        const QFileInfo info(base);
        const QString directory = info.absolutePath();
        const QString suffix = info.suffix().isEmpty() ? QStringLiteral("mov") : info.suffix();
        const QString stem =
            info.completeBaseName().isEmpty() ? QStringLiteral("render") : info.completeBaseName();
        const auto used = [this](const QString& path) {
            const QString absolute = QFileInfo(path).absoluteFilePath();
            for (const RenderJob& job : jobs_) {
                if (QFileInfo(job.output).absoluteFilePath() == absolute) return true;
            }
            return false;
        };
        QString candidate = base;
        int copy = 2;
        while (used(candidate)) {
            candidate = directory + QLatin1Char('/') + stem + QLatin1Char('-') +
                        QString::number(copy++) + QLatin1Char('.') + suffix;
        }
        return candidate;
    }

    QString createLutSnapshot() {
        const QString queueDir = repoRoot_ + QStringLiteral("/out/ui-queue");
        QDir().mkpath(queueDir);
        const QString snapshot =
            queueDir + QStringLiteral("/lut-%1-%2.cube")
                           .arg(QDateTime::currentMSecsSinceEpoch())
                           .arg(++lutCounter_);
        if (!QFile::copy(lutEdit_->text(), snapshot)) {
            QMessageBox::critical(this, QStringLiteral("入队失败"),
                                  QStringLiteral("无法复制 LUT 到队列目录：\n") + snapshot);
            return QString();
        }
        return snapshot;
    }

    void addQueueJob(const QString& input, const QString& output, const QString& snapshot) {
        RenderJob job;
        job.input = input;
        job.output = output;
        job.lutSnapshot = snapshot;
        job.args = buildVideoArgs(input, output, snapshot);

        long total = frames_->value();
        if (total <= 0) {
            const ClipInfo info = probeClip(input);
            if (info.frames > 0 && info.fps > 0.0) {
                const long startFrame =
                    static_cast<long>(std::llround(start_->value() * info.fps));
                total = std::max(0L, info.frames - startFrame);
            }
        }
        job.total = static_cast<int>(total);
        jobs_.append(job);
    }

    QString queueFilePath() const {
        return repoRoot_ + QStringLiteral("/out/ui-queue/queue.json");
    }

    void saveQueue() const {
        const QString path = queueFilePath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        QJsonArray array;
        for (const RenderJob& job : jobs_) {
            QJsonObject object;
            object.insert(QStringLiteral("input"), job.input);
            object.insert(QStringLiteral("output"), job.output);
            object.insert(QStringLiteral("lut"), job.lutSnapshot);
            QJsonArray args;
            for (const QString& argument : job.args) args.append(argument);
            object.insert(QStringLiteral("args"), args);
            object.insert(QStringLiteral("status"), job.status);
            object.insert(QStringLiteral("done"), job.done);
            object.insert(QStringLiteral("total"), job.total);
            object.insert(QStringLiteral("started"), static_cast<double>(job.startedMs));
            object.insert(QStringLiteral("finished"), static_cast<double>(job.finishedMs));
            array.append(object);
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    }

    void loadQueue() {
        QFile file(queueFilePath());
        if (!file.open(QIODevice::ReadOnly)) return;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isArray()) return;
        for (const QJsonValue& value : document.array()) {
            const QJsonObject object = value.toObject();
            RenderJob job;
            job.input = object.value(QStringLiteral("input")).toString();
            job.output = object.value(QStringLiteral("output")).toString();
            job.lutSnapshot = object.value(QStringLiteral("lut")).toString();
            for (const QJsonValue& argument : object.value(QStringLiteral("args")).toArray()) {
                job.args << argument.toString();
            }
            job.status = object.value(QStringLiteral("status")).toString(QStringLiteral("等待"));
            if (job.status == QStringLiteral("渲染中")) {
                job.status = QStringLiteral("等待");
            }
            job.done = object.value(QStringLiteral("done")).toInt();
            job.total = object.value(QStringLiteral("total")).toInt();
            job.startedMs =
                static_cast<qint64>(object.value(QStringLiteral("started")).toDouble());
            job.finishedMs =
                static_cast<qint64>(object.value(QStringLiteral("finished")).toDouble());
            if (job.status == QStringLiteral("等待")) {
                job.done = 0;
                job.startedMs = 0;
                job.finishedMs = 0;
            }
            if (job.input.isEmpty() || job.output.isEmpty() || job.args.isEmpty()) continue;
            bool spectralJob = false;
            for (int i = 0; i + 1 < job.args.size(); ++i) {
                if (job.args[i] == QStringLiteral("--engine") &&
                    (job.args[i + 1] == QStringLiteral("spectral") ||
                     job.args[i + 1] == QStringLiteral("spectral-gpu"))) {
                    spectralJob = true;
                }
            }
            if (!spectralJob &&
                (job.lutSnapshot.isEmpty() || !QFileInfo::exists(job.lutSnapshot))) {
                continue;
            }
            jobs_.append(job);
        }
        refreshQueueTable();
        if (!jobs_.isEmpty()) {
            status_->setText(QStringLiteral("已恢复 %1 条队列任务").arg(jobs_.size()));
        }
    }

    void enqueueBatch(const QStringList& videos) {
        if (videos.isEmpty()) return;
        for (const QString& video : videos) {
            if (isRedPath(video)) {
                QMessageBox::information(
                    this, QStringLiteral("批量 R3D 还没接"),
                    QStringLiteral("单条渲染/入队已经支持 R3D；批量先一次选一个，"
                                   "或用 build-red 的 red-decode --out-dir。"));
                return;
            }
        }
        if (rendering_) {
            QMessageBox::information(this, QStringLiteral("正在渲染"),
                                     QStringLiteral("等当前单条渲染结束再入队。"));
            return;
        }
        if (!ensureLut()) return;
        QString base = outputEdit_->text().trimmed();
        if (base.isEmpty()) base = repoRoot_ + QStringLiteral("/out/render.mov");
        const QFileInfo baseInfo(base);
        const QString directory = baseInfo.absolutePath();
        QDir().mkpath(directory);
        const QString suffix = baseInfo.suffix().isEmpty() ? QStringLiteral("mov")
                                                           : baseInfo.suffix();
        const QString snapshot = spectralEngine() ? QString() : createLutSnapshot();
        if (!spectralEngine() && snapshot.isEmpty()) return;
        for (const QString& video : videos) {
            const QString stem = QFileInfo(video).completeBaseName();
            const QString output = uniqueOutputPath(directory + QLatin1Char('/') + stem +
                                                    QStringLiteral("-graded.") + suffix);
            addQueueJob(video, output, snapshot);
        }
        refreshQueueTable();
        saveQueue();
        tabs_->setCurrentIndex(queueTabIndex_);
        status_->setText(QStringLiteral("批量加入 %1 条任务，点“开始队列”").arg(videos.size()));
    }

    void enqueueJob() {
        if (!requireInput() || !ensureLut()) return;
        if (rendering_) {
            QMessageBox::information(this, QStringLiteral("正在渲染"),
                                     QStringLiteral("等当前单条渲染结束再入队。"));
            return;
        }
        QString output = outputEdit_->text().trimmed();
        if (output.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("缺少输出"),
                                 QStringLiteral("请先指定输出文件。"));
            return;
        }
        output = uniqueOutputPath(output);
        QDir().mkpath(QFileInfo(output).absolutePath());

        const QString snapshot = spectralEngine() ? QString() : createLutSnapshot();
        if (!spectralEngine() && snapshot.isEmpty()) return;
        addQueueJob(inputEdit_->text(), output, snapshot);

        refreshQueueTable();
        saveQueue();
        tabs_->setCurrentIndex(queueTabIndex_);
        queueTable_->selectRow(jobs_.size() - 1);
        status_->setText(QStringLiteral("已加入队列：") + QFileInfo(output).fileName());
    }

    void updateJobRow(int row) {
        if (row < 0 || row >= jobs_.size()) return;
        const RenderJob& job = jobs_[row];
        const auto setCell = [this, row](int column, const QString& text) {
            QTableWidgetItem* item = queueTable_->item(row, column);
            if (item == nullptr) {
                item = new QTableWidgetItem;
                queueTable_->setItem(row, column, item);
            }
            item->setText(text);
        };
        setCell(0, QFileInfo(job.input).fileName());
        setCell(1, QFileInfo(job.output).fileName());
        setCell(2, job.status);

        auto* bar = qobject_cast<QProgressBar*>(queueTable_->cellWidget(row, 3));
        if (bar == nullptr) {
            bar = new QProgressBar;
            bar->setTextVisible(true);
            bar->setAlignment(Qt::AlignCenter);
            bar->setMaximumHeight(18);
            queueTable_->setCellWidget(row, 3, bar);
        }
        if (job.status == QStringLiteral("渲染中") && job.total <= 0) {
            bar->setRange(0, 0);
            bar->setFormat(QStringLiteral("渲染中"));
        } else if (job.total > 0) {
            bar->setRange(0, job.total);
            bar->setValue(job.done);
            bar->setFormat(QStringLiteral("%v/%m"));
        } else if (job.status == QStringLiteral("完成")) {
            bar->setRange(0, 100);
            bar->setValue(100);
            bar->setFormat(QStringLiteral("完成"));
        } else {
            bar->setRange(0, 100);
            bar->setValue(0);
            bar->setFormat(QStringLiteral("—"));
        }

        QString timeText = QStringLiteral("—");
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (job.status == QStringLiteral("渲染中") && job.startedMs > 0) {
            const qint64 elapsed = now - job.startedMs;
            if (job.done > 0 && job.total > job.done) {
                const qint64 eta = elapsed * (job.total - job.done) / job.done;
                timeText = formatDuration(elapsed) + QStringLiteral(" / 剩 ") + formatDuration(eta);
            } else {
                timeText = formatDuration(elapsed);
            }
        } else if (job.startedMs > 0 && job.status != QStringLiteral("等待")) {
            const qint64 end = job.finishedMs > 0 ? job.finishedMs : now;
            timeText = formatDuration(end - job.startedMs);
        }
        setCell(4, timeText);

        queueTable_->item(row, 0)->setToolTip(job.input);
        queueTable_->item(row, 1)->setToolTip(job.output);

        QColor color(200, 200, 200);
        if (job.status == QStringLiteral("渲染中")) color = QColor(240, 200, 90);
        else if (job.status == QStringLiteral("完成")) color = QColor(120, 210, 120);
        else if (job.status == QStringLiteral("失败")) color = QColor(235, 110, 110);
        else if (job.status == QStringLiteral("已取消")) color = QColor(230, 160, 100);
        queueTable_->item(row, 2)->setForeground(color);
    }

    void refreshQueueTable() {
        queueTable_->setRowCount(jobs_.size());
        for (int row = 0; row < jobs_.size(); ++row) updateJobRow(row);
        updateQueueButtons();
    }

    void updateQueueButtons() {
        bool hasWaiting = false;
        bool hasFinished = false;
        for (const RenderJob& job : jobs_) {
            if (job.status == QStringLiteral("等待")) hasWaiting = true;
            if (job.status != QStringLiteral("等待") && job.status != QStringLiteral("渲染中")) {
                hasFinished = true;
            }
        }
        const int selected = queueTable_->currentRow();
        queueStartButton_->setEnabled(!queueRunning_ && !rendering_ && hasWaiting &&
                                      queueProcess_ == nullptr);
        queueStopButton_->setEnabled(queueRunning_);
        queueRemoveButton_->setEnabled(!queueRunning_ && selected >= 0);
        queueClearButton_->setEnabled(!queueRunning_ && hasFinished);
        queueOpenButton_->setEnabled(selected >= 0 && selected < jobs_.size());
        if (batchAddButton_ != nullptr) batchAddButton_->setEnabled(!rendering_);
    }

    void startQueue() {
        if (queueRunning_ || rendering_) return;
        bool hasWaiting = false;
        for (const RenderJob& job : jobs_) {
            if (job.status == QStringLiteral("等待")) hasWaiting = true;
        }
        if (!hasWaiting) {
            status_->setText(QStringLiteral("队列里没有等待的任务"));
            return;
        }
        queueRunning_ = true;
        queueCanceled_ = false;
        setBusy(true);
        cancelButton_->setEnabled(false);
        progress_->setRange(0, 0);
        tabs_->setCurrentIndex(queueTabIndex_);
        updateQueueButtons();
        etaTimer_->start();
        startNextJob();
    }

    void stopQueue() {
        if (!queueRunning_) return;
        queueRunning_ = false;
        queueCanceled_ = true;
        status_->setText(QStringLiteral("正在停止队列…"));
        if (queueProcess_ != nullptr) queueProcess_->kill();
        updateQueueButtons();
    }

    void startNextJob() {
        if (!queueRunning_) return;
        int index = -1;
        for (int i = 0; i < jobs_.size(); ++i) {
            if (jobs_[i].status == QStringLiteral("等待")) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            int ok = 0;
            int failed = 0;
            for (const RenderJob& job : jobs_) {
                if (job.status == QStringLiteral("完成")) ++ok;
                if (job.status == QStringLiteral("失败")) ++failed;
            }
            queueRunning_ = false;
            activeJob_ = -1;
            setBusy(false);
            etaTimer_->stop();
            progress_->setRange(0, 100);
            progress_->setValue(100);
            status_->setText(QStringLiteral("队列完成：%1 成功，%2 失败").arg(ok).arg(failed));
            updateQueueButtons();
            return;
        }

        activeJob_ = index;
        jobs_[index].status = QStringLiteral("渲染中");
        jobs_[index].done = 0;
        jobs_[index].startedMs = QDateTime::currentMSecsSinceEpoch();
        jobs_[index].finishedMs = 0;
        updateJobRow(index);
        queueTable_->selectRow(index);
        updateQueueButtons();
        saveQueue();

        log_->clear();
        appendLog(QStringLiteral("> ") + cliPath_ + QLatin1Char(' ') +
                  jobs_[index].args.join(QLatin1Char(' ')) + QLatin1Char('\n'));
        pending_.clear();
        queueCanceled_ = false;
        progress_->setRange(0, 0);
        status_->setText(QStringLiteral("队列 %1/%2：渲染中…")
                             .arg(index + 1)
                             .arg(jobs_.size()));

        auto* process = new QProcess(this);
        queueProcess_ = process;
        process->setWorkingDirectory(repoRoot_);
        process->setProcessChannelMode(QProcess::MergedChannels);
        connect(process, &QProcess::readyRead, this, [this] { drainQueueOutput(); });
        connect(process, &QProcess::finished, this,
                [this, process](int code, QProcess::ExitStatus exitStatus) {
                    drainQueueOutput();
                    const int finishedIndex = activeJob_;
                    if (finishedIndex >= 0 && finishedIndex < jobs_.size()) {
                        const bool ok =
                            !queueCanceled_ && exitStatus == QProcess::NormalExit && code == 0;
                        jobs_[finishedIndex].status =
                            queueCanceled_ ? QStringLiteral("已取消")
                                           : (ok ? QStringLiteral("完成")
                                                 : QStringLiteral("失败"));
                        if (ok && jobs_[finishedIndex].total > 0) {
                            jobs_[finishedIndex].done = jobs_[finishedIndex].total;
                        }
                        jobs_[finishedIndex].finishedMs = QDateTime::currentMSecsSinceEpoch();
                        updateJobRow(finishedIndex);
                        saveQueue();
                    }
                    activeJob_ = -1;
                    queueProcess_ = nullptr;
                    process->deleteLater();
                    if (queueRunning_) {
                        startNextJob();
                    } else {
                        setBusy(false);
                        etaTimer_->stop();
                        progress_->setRange(0, 100);
                        status_->setText(QStringLiteral("队列已停止；未完成的任务仍是“等待”"));
                        updateQueueButtons();
                    }
                });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
            if (!canceled_ && !queueCanceled_) {
                appendLog(QStringLiteral("进程错误：") + process->errorString() +
                          QLatin1Char('\n'));
            }
        });
        process->start(cliPath_, jobs_[index].args);
        if (!process->waitForStarted(5000)) {
            appendLog(QStringLiteral("无法启动 CLI：") + process->errorString() +
                      QLatin1Char('\n'));
            jobs_[index].status = QStringLiteral("失败");
            updateJobRow(index);
            queueProcess_ = nullptr;
            activeJob_ = -1;
            process->deleteLater();
            startNextJob();
        }
    }

    void drainQueueOutput() {
        if (queueProcess_ == nullptr) return;
        const QString text = QString::fromLocal8Bit(queueProcess_->readAll());
        if (text.isEmpty()) return;
        appendLog(text);
        pending_ += text;
        const QRegularExpression framesRe(QStringLiteral("(\\d+)\\s*/\\s*(\\d+)\\s+frames"));
        const QRegularExpression wroteRe(QStringLiteral("wrote .*?: (\\d+) frames"));
        int newline = 0;
        while ((newline = pending_.indexOf(QLatin1Char('\n'))) >= 0) {
            const QString line = pending_.left(newline).trimmed();
            pending_.remove(0, newline + 1);
            if (line.isEmpty()) continue;
            status_->setText(statusLine(line));
            const auto match = framesRe.match(line);
            if (match.hasMatch() && activeJob_ >= 0 && activeJob_ < jobs_.size()) {
                jobs_[activeJob_].done = match.captured(1).toInt();
                jobs_[activeJob_].total = match.captured(2).toInt();
                updateJobRow(activeJob_);
                continue;
            }
            const auto wrote = wroteRe.match(line);
            if (wrote.hasMatch() && activeJob_ >= 0 && activeJob_ < jobs_.size()) {
                const int frames = wrote.captured(1).toInt();
                jobs_[activeJob_].done = frames;
                jobs_[activeJob_].total = frames;
                updateJobRow(activeJob_);
            }
        }
    }

    void releaseSnapshotIfUnused(const QString& snapshot) {
        if (snapshot.isEmpty()) return;
        for (const RenderJob& job : jobs_) {
            if (job.lutSnapshot == snapshot) return;
        }
        QFile::remove(snapshot);
    }

    void removeJobAt(int row) {
        if (row < 0 || row >= jobs_.size() || queueRunning_) return;
        const QString snapshot = jobs_[row].lutSnapshot;
        jobs_.removeAt(row);
        refreshQueueTable();
        saveQueue();
        releaseSnapshotIfUnused(snapshot);
    }

    void removeSelectedJob() {
        removeJobAt(queueTable_->currentRow());
    }

    void clearFinishedJobs() {
        if (queueRunning_) return;
        for (int row = jobs_.size() - 1; row >= 0; --row) {
            if (jobs_[row].status != QStringLiteral("等待") &&
                jobs_[row].status != QStringLiteral("渲染中")) {
                const QString snapshot = jobs_[row].lutSnapshot;
                jobs_.removeAt(row);
                releaseSnapshotIfUnused(snapshot);
            }
        }
        refreshQueueTable();
        saveQueue();
    }

    void openSelectedOutput() {
        const int row = queueTable_->currentRow();
        if (row < 0 || row >= jobs_.size()) return;
        const QFileInfo info(jobs_[row].output);
        if (!info.exists()) {
            QMessageBox::information(this, QStringLiteral("还没有输出"),
                                     QStringLiteral("这个任务还没有生成文件。"));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
    }

    void setBusy(bool busy) {
        bakeButton_->setEnabled(!busy);
        previewButton_->setEnabled(!busy);
        renderButton_->setEnabled(!busy && !queueRunning_);
        addQueueButton_->setEnabled(!rendering_);
        cancelButton_->setEnabled(busy && !queueRunning_);
    }

    bool runSync(const QString& program, const QStringList& args, QString* output) {
        if (program.trimmed().isEmpty()) {
            *output = QStringLiteral("程序路径为空");
            return false;
        }
        QProcess process;
        process.setWorkingDirectory(repoRoot_);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(program, args);
        if (!process.waitForStarted(10000)) {
            *output = QStringLiteral("无法启动：") + program;
            return false;
        }
        process.waitForFinished(-1);
        *output = QString::fromLocal8Bit(process.readAll());
        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

    void appendLog(const QString& text) {
        if (text.isEmpty()) return;
        log_->moveCursor(QTextCursor::End);
        log_->insertPlainText(text);
        log_->moveCursor(QTextCursor::End);
    }

    // CLI warnings are informational (e.g. a GPU fallback). Show them as a note,
    // not as a bare "warning: ..." that reads like a render failure (D066).
    // D081 removed the OCIO + spectral-gpu fallback, so it has no special text.
    static QString statusLine(const QString& line) {
        if (line.startsWith(QStringLiteral("warning:"))) {
            return QStringLiteral("提示：") + line.mid(8).trimmed();
        }
        return line;
    }

    // ---- state -----------------------------------------------------------

    QString repoRoot_;
    QString cliPath_;

    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* ffmpegEdit_ = nullptr;
    QLineEdit* lutEdit_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;

    QComboBox* source_ = nullptr;
    // D071: R3D decode scale and state. The UI only drives the CLI.
    QComboBox* redScale_ = nullptr;
    bool redInput_ = false;
    bool redAvailable_ = false;
    // OpenColorIO input (D063).
    QCheckBox* ocioEnabled_ = nullptr;
    QLineEdit* ocioConfig_ = nullptr;
    QComboBox* ocioSpace_ = nullptr;
    QPushButton* ocioRefreshButton_ = nullptr;
    QLabel* ocioInfoLabel_ = nullptr;
    QComboBox* filmCombo_ = nullptr;
    QComboBox* paperCombo_ = nullptr;
    QDoubleSpinBox* exposure_ = nullptr;
    QComboBox* scan_ = nullptr;
    QDoubleSpinBox* contrast_ = nullptr;
    QComboBox* trc_ = nullptr;
    QCheckBox* couplers_ = nullptr;
    QCheckBox* gamut_ = nullptr;

    QDoubleSpinBox* grain_ = nullptr;
    QSpinBox* grainRate_ = nullptr;
    QDoubleSpinBox* halation_ = nullptr;
    QDoubleSpinBox* glare_ = nullptr;
    QDoubleSpinBox* diffusion_ = nullptr;
    QCheckBox* textureGpu_ = nullptr;

    QComboBox* codec_ = nullptr;
    QComboBox* engine_ = nullptr;
    QDoubleSpinBox* start_ = nullptr;
    QSpinBox* frames_ = nullptr;
    QSpinBox* segment_ = nullptr;
    QCheckBox* resume_ = nullptr;
    QDoubleSpinBox* previewTime_ = nullptr;

    QPushButton* bakeButton_ = nullptr;
    QPushButton* lutBrowseButton_ = nullptr;
    QPushButton* previewButton_ = nullptr;
    QPushButton* compareButton_ = nullptr;
    QPushButton* renderButton_ = nullptr;
    QPushButton* addQueueButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* queueStartButton_ = nullptr;
    QPushButton* queueStopButton_ = nullptr;
    QPushButton* batchAddButton_ = nullptr;
    QPushButton* queueRemoveButton_ = nullptr;
    QPushButton* queueClearButton_ = nullptr;
    QPushButton* queueOpenButton_ = nullptr;
    QPushButton* fitButton_ = nullptr;
    QPushButton* zoom100Button_ = nullptr;

    QProgressBar* progress_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* clipNameLabel_ = nullptr;
    QLabel* clipInfoLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QSlider* timeline_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTableWidget* queueTable_ = nullptr;
    PreviewView* preview_ = nullptr;

    ClipInfo clipInfo_;
    QImage gradedImage_;
    QImage originalImage_;
    double originalTime_ = -1.0;
    QString lastProbePath_;

    // Preview server state (D048).
    QProcess* previewServer_ = nullptr;
    QTimer* scrubTimer_ = nullptr;
    bool scrubbing_ = false;
    // YUV -> RGB matrix the running server was started with (D061); the server
    // is restarted when a clip with a different container tag is opened.
    QString previewMatrix_;
    QString previewLook_;
    QString previewClip_;
    QString previewTexture_;
    QString previewBackend_;
    int previewWidth_ = 0;
    int previewHeight_ = 0;
    int previewFrames_ = 0;

    QVector<RenderJob> jobs_;
    bool queueRunning_ = false;
    bool queueCanceled_ = false;
    int activeJob_ = -1;
    int lutCounter_ = 0;
    int queueTabIndex_ = 0;
    QProcess* queueProcess_ = nullptr;
    QTimer* etaTimer_ = nullptr;

    QProcess* renderProcess_ = nullptr;
    QString pending_;
    bool rendering_ = false;
    bool canceled_ = false;
    // D071 selftests run the real button path but must not block on the
    // "open the output folder?" modal.
    bool suppressCompletionDialog_ = false;
    bool geometryChecked_ = false;
    bool lutBaked_ = false;
    bool lutStale_ = false;
};

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SpektraRoll"));
    QCoreApplication::setApplicationName(QStringLiteral("SpektraRoll UI"));
    QApplication::setStyle(QStringLiteral("Fusion"));

    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor(0x2d, 0x2d, 0x2c));
    palette.setColor(QPalette::WindowText, QColor(0xfa, 0xf8, 0xf4));
    palette.setColor(QPalette::Base, QColor(0x48, 0x48, 0x46));
    palette.setColor(QPalette::AlternateBase, QColor(0x25, 0x25, 0x23));
    palette.setColor(QPalette::Text, QColor(0xfa, 0xf8, 0xf4));
    palette.setColor(QPalette::Button, QColor(0x48, 0x48, 0x46));
    palette.setColor(QPalette::ButtonText, QColor(0xfa, 0xf8, 0xf4));
    palette.setColor(QPalette::Highlight, QColor(0xe1, 0xa9, 0x5f));
    palette.setColor(QPalette::HighlightedText, QColor(0x25, 0x25, 0x23));
    palette.setColor(QPalette::ToolTipBase, QColor(0x1e, 0x1e, 0x1c));
    palette.setColor(QPalette::ToolTipText, QColor(0xfa, 0xf8, 0xf4));
    palette.setColor(QPalette::PlaceholderText, QColor(0x8c, 0x8a, 0x86));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x77, 0x77, 0x73));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x77, 0x77, 0x73));
    app.setPalette(palette);
    app.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    app.setStyleSheet(QString::fromUtf8(kStyleSheet));

    MainWindow window;
    const QStringList arguments = QCoreApplication::arguments();
    // The OCIO and stock checks only read JSON/bake a LUT; do not flash a window.
    if (!arguments.contains(QStringLiteral("--ocio-selftest")) &&
        !arguments.contains(QStringLiteral("--stocks-selftest")) &&
        !arguments.contains(QStringLiteral("--red-selftest")) &&
        !arguments.contains(QStringLiteral("--red-render-selftest"))) {
        window.show();
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &window,
                     [&window] { window.shutdownPreview(); });

    // Diagnostic hook for scripts and CI: run one real preview through the same
    // code the button uses, print the status line, then exit.
    if (arguments.contains(QStringLiteral("--preview-selftest"))) {
        QTimer::singleShot(400, &window, [&window] {
            const QString first = window.previewSelfTest();
            const QString second = window.previewSelfTest();
            std::printf("selftest first:  %s\n", first.toUtf8().constData());
            std::printf("selftest second: %s\n", second.toUtf8().constData());
            std::fflush(stdout);
            QCoreApplication::quit();
        });
    }
    // D063: OCIO wiring without clicking: list the config, bake a LUT.
    if (arguments.contains(QStringLiteral("--ocio-selftest"))) {
        QTimer::singleShot(400, &window, [&window, &app] {
            const QString result = window.ocioSelfTest();
            std::printf("ocio selftest: %s\n", result.toUtf8().constData());
            std::fflush(stdout);
            window.shutdownPreview();
            app.exit(result.startsWith(QStringLiteral("ok")) ? 0 : 1);
        });
    }
    // D065: check the profile catalogue the UI builds.
    if (arguments.contains(QStringLiteral("--stocks-selftest"))) {
        QTimer::singleShot(400, &window, [&window, &app] {
            const QString result = window.stocksSelfTest();
            std::printf("stocks selftest: %s\n", result.toUtf8().constData());
            std::fflush(stdout);
            window.shutdownPreview();
            app.exit(result.startsWith(QStringLiteral("ok")) ? 0 : 1);
        });
    }
    // D071: R3D probe + LUT bake + preview through the UI path.
    if (arguments.contains(QStringLiteral("--red-selftest"))) {
        QTimer::singleShot(800, &window, [&window, &app] {
            const QString result = window.redSelfTest();
            std::printf("red selftest: %s\n", result.toUtf8().constData());
            std::fflush(stdout);
            window.shutdownPreview();
            app.exit(result.startsWith(QStringLiteral("ok")) ? 0 : 1);
        });
    }
    // D071: the actual Render button path for two R3D frames.
    if (arguments.contains(QStringLiteral("--red-render-selftest"))) {
        QTimer::singleShot(800, &window, [&window, &app] {
            const QString result = window.redRenderSelfTest();
            std::printf("red render selftest: %s\n", result.toUtf8().constData());
            std::fflush(stdout);
            window.shutdownPreview();
            app.exit(result.startsWith(QStringLiteral("ok")) ? 0 : 1);
        });
    }
    return app.exec();
}
