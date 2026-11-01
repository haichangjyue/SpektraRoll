// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
//
// Qt 6 desktop shell (D009). The window deliberately does not reimplement the
// colour or texture pipeline: preview and render both drive the same
// `spektra-roll-cli` binary that CI exercises, so the UI cannot drift from the
// command line. Qt is dynamically linked and deployed with windeployqt.

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
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
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>
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

struct ClipInfo {
    int width = 0;
    int height = 0;
    double fps = 0.0;
    double duration = 0.0;
    long frames = 0;
    QString timecode;
    bool valid() const { return width > 0 && height > 0 && fps > 0.0; }
};

class PreviewView : public QWidget {
public:
    explicit PreviewView(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(480, 300);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setImage(const QImage& image) {
        image_ = image;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(18, 18, 20));
        if (image_.isNull()) {
            painter.setPen(QColor(150, 150, 150));
            painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("预览会显示在这里"));
            return;
        }
        const QSize scaled = image_.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target(QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2),
                           scaled);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(target, image_);
    }

private:
    QImage image_;
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
        setWindowTitle(QStringLiteral("SpektraRoll — 胶片与印片模拟"));
    }

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
        for (const QUrl& url : event->mimeData()->urls()) {
            const QString path = url.toLocalFile();
            if (path.isEmpty()) continue;
            inputEdit_->setText(QDir::toNativeSeparators(path));
            probeInput();
            event->acceptProposedAction();
            return;
        }
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
        saveSettings();
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
        const QString local =
            QCoreApplication::applicationDirPath() + "/spektra-roll-cli" + exeSuffix();
        if (QFileInfo::exists(local)) return local;
        const QString fallback = repoRoot_ + "/build/spektra-roll-cli" + exeSuffix();
        if (QFileInfo::exists(fallback)) return fallback;
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
        const QString known =
            QStringLiteral("X:/agent/footage/A001_11011538_C001.mov");
        return QFileInfo::exists(known) ? known : QString();
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

    bool runCaptureSync(const QString& program, const QStringList& args, QString* output) const {
        if (program.trimmed().isEmpty()) {
            *output = QString();
            return false;
        }
        QProcess process;
        process.setWorkingDirectory(repoRoot_);
        process.start(program, args);
        if (!process.waitForStarted(10000)) {
            *output = QString();
            return false;
        }
        process.waitForFinished(-1);
        *output = QString::fromLocal8Bit(process.readAllStandardOutput());
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
        auto* left = new QWidget;
        auto* leftLayout = new QVBoxLayout(left);
        leftLayout->setContentsMargins(12, 12, 12, 12);

        inputEdit_ = new QLineEdit;
        ffmpegEdit_ = new QLineEdit;
        lutEdit_ = new QLineEdit;
        outputEdit_ = new QLineEdit;

        auto* sourceGroup = new QGroupBox(QStringLiteral("素材与胶片"));
        auto* sourceForm = new QFormLayout(sourceGroup);
        sourceForm->addRow(QStringLiteral("视频素材"), pathRow(inputEdit_, QStringLiteral("选择视频"),
                                                            QStringLiteral("视频 (*.mov *.mp4 *.mxf)")));
        source_ = new QComboBox;
        source_->addItem(QStringLiteral("Apple Log（iPhone）"), QStringLiteral("apple-log"));
        source_->addItem(QStringLiteral("普通 SDR 素材"), QStringLiteral("srgb"));
        sourceForm->addRow(QStringLiteral("画面类型"), source_);
        sourceForm->addRow(QStringLiteral("ffmpeg"),
                           pathRow(ffmpegEdit_, QStringLiteral("选择 ffmpeg"),
                                   QStringLiteral("ffmpeg (ffmpeg.exe)")));

        auto* lutRow = new QWidget;
        auto* lutLayout = new QHBoxLayout(lutRow);
        lutLayout->setContentsMargins(0, 0, 0, 0);
        lutEdit_ = new QLineEdit;
        lutLayout->addWidget(lutEdit_, 1);
        auto* lutBrowse = new QPushButton(QStringLiteral("…"));
        lutBrowse->setFixedWidth(34);
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
        sourceForm->addRow(QStringLiteral("LUT"), lutRow);

        exposure_ = new QDoubleSpinBox;
        exposure_->setRange(-6.0, 6.0);
        exposure_->setSingleStep(0.1);
        exposure_->setDecimals(2);
        sourceForm->addRow(QStringLiteral("曝光 (EV)"), exposure_);

        scan_ = new QComboBox;
        scan_->addItem(QStringLiteral("扫描负片（推荐）"), QStringLiteral("negative"));
        scan_->addItem(QStringLiteral("印片"), QStringLiteral("print"));
        sourceForm->addRow(QStringLiteral("扫描方式"), scan_);

        contrast_ = new QDoubleSpinBox;
        contrast_->setRange(0.5, 2.0);
        contrast_->setSingleStep(0.05);
        contrast_->setDecimals(2);
        sourceForm->addRow(QStringLiteral("反差"), contrast_);

        trc_ = new QComboBox;
        trc_->addItem(QStringLiteral("SDR"), QStringLiteral("srgb"));
        trc_->addItem(QStringLiteral("HDR · PQ"), QStringLiteral("pq"));
        trc_->addItem(QStringLiteral("HDR · HLG"), QStringLiteral("hlg"));
        sourceForm->addRow(QStringLiteral("输出"), trc_);

        auto* textureGroup = new QGroupBox(QStringLiteral("胶片质感"));
        auto* textureForm = new QFormLayout(textureGroup);
        grain_ = amountSpin(0.4);
        textureForm->addRow(QStringLiteral("颗粒"), grain_);
        grainRate_ = new QSpinBox;
        grainRate_->setRange(1, 12);
        grainRate_->setValue(3);
        grainRate_->setToolTip(QStringLiteral("一个颗粒场持续几帧；3 最接近真胶片，1 会闪。"));
        textureForm->addRow(QStringLiteral("颗粒稳定度"), grainRate_);
        halation_ = amountSpin(0.45);
        textureForm->addRow(QStringLiteral("光晕"), halation_);
        glare_ = amountSpin(0.2);
        textureForm->addRow(QStringLiteral("眩光"), glare_);
        diffusion_ = amountSpin(0.1);
        textureForm->addRow(QStringLiteral("扩散"), diffusion_);

        auto* renderGroup = new QGroupBox(QStringLiteral("渲染"));
        auto* renderForm = new QFormLayout(renderGroup);
        renderForm->addRow(QStringLiteral("输出文件"),
                           saveRow(outputEdit_, QStringLiteral("保存视频"),
                                   QStringLiteral("视频 (*.mov *.mp4)")));
        codec_ = new QComboBox;
        codec_->addItem(QStringLiteral("H.264"), QStringLiteral("h264"));
        codec_->addItem(QStringLiteral("HEVC (HDR 推荐)"), QStringLiteral("hevc"));
        codec_->addItem(QStringLiteral("ProRes 422 HQ"), QStringLiteral("prores"));
        renderForm->addRow(QStringLiteral("编码"), codec_);

        start_ = new QDoubleSpinBox;
        start_->setRange(0.0, 86400.0);
        start_->setDecimals(3);
        start_->setSuffix(QStringLiteral(" s"));
        renderForm->addRow(QStringLiteral("起始"), start_);

        frames_ = new QSpinBox;
        frames_->setRange(0, 10000000);
        frames_->setSpecialValueText(QStringLiteral("整段"));
        renderForm->addRow(QStringLiteral("帧数"), frames_);

        segment_ = new QSpinBox;
        segment_->setRange(0, 3600);
        segment_->setSuffix(QStringLiteral(" s"));
        segment_->setSpecialValueText(QStringLiteral("不分段"));
        segment_->setValue(10);
        renderForm->addRow(QStringLiteral("分段长度"), segment_);

        resume_ = new QCheckBox(QStringLiteral("中断后续渲（跳过已完成的段）"));
        resume_->setChecked(true);
        renderForm->addRow(QString(), resume_);

        previewTime_ = new QDoubleSpinBox;
        previewTime_->setRange(0.0, 86400.0);
        previewTime_->setDecimals(2);
        previewTime_->setSuffix(QStringLiteral(" s"));
        previewTime_->setValue(1.5);
        renderForm->addRow(QStringLiteral("预览时间"), previewTime_);

        auto* buttons = new QHBoxLayout;
        previewButton_ = new QPushButton(QStringLiteral("预览一帧"));
        compareButton_ = new QPushButton(QStringLiteral("按住看原片"));
        compareButton_->setToolTip(QStringLiteral("按住显示未套胶片的原片，松开回到成片"));
        previewButton_->setToolTip(QStringLiteral("Ctrl+P"));
        renderButton_ = new QPushButton(QStringLiteral("渲染整段"));
        renderButton_->setDefault(true);
        renderButton_->setToolTip(QStringLiteral("Ctrl+R"));
        cancelButton_ = new QPushButton(QStringLiteral("取消"));
        cancelButton_->setEnabled(false);
        buttons->addWidget(previewButton_);
        buttons->addWidget(compareButton_);

        auto* buttons2 = new QHBoxLayout;
        auto* folderButton = new QPushButton(QStringLiteral("输出文件夹"));
        folderButton->setToolTip(QStringLiteral("在资源管理器里打开输出文件所在目录"));
        buttons2->addWidget(renderButton_);
        buttons2->addWidget(cancelButton_);
        buttons2->addWidget(folderButton);

        progress_ = new QProgressBar;
        progress_->setRange(0, 100);
        progress_->setValue(0);
        progress_->setTextVisible(true);
        status_ = new QLabel(QStringLiteral("就绪"));
        status_->setWordWrap(true);

        leftLayout->addWidget(sourceGroup);
        leftLayout->addWidget(textureGroup);
        leftLayout->addWidget(renderGroup);
        leftLayout->addLayout(buttons);
        leftLayout->addLayout(buttons2);
        leftLayout->addWidget(progress_);
        leftLayout->addWidget(status_);
        leftLayout->addStretch(1);

        connect(previewButton_, &QPushButton::clicked, this, [this] { previewFrame(); });
        connect(renderButton_, &QPushButton::clicked, this, [this] { startRender(); });
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
        connect(source_, &QComboBox::currentIndexChanged, this, [this](int) {
            originalImage_ = QImage();
            originalTime_ = -1.0;
            markLookChanged();
        });
        connect(scan_, &QComboBox::currentIndexChanged, this,
                [this](int) { markLookChanged(); });
        connect(trc_, &QComboBox::currentIndexChanged, this,
                [this](int) { markLookChanged(); });

        auto* scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setWidget(left);
        scroll->setMinimumWidth(430);

        preview_ = new PreviewView;
        clipInfoLabel_ = new QLabel(QStringLiteral("未选择素材"));
        clipInfoLabel_->setStyleSheet(QStringLiteral("color: #b8b8b8;"));
        clipInfoLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

        timeline_ = new QSlider(Qt::Horizontal);
        timeline_->setRange(0, 0);
        timeline_->setToolTip(QStringLiteral("拖动到某一帧，松开后刷新预览"));
        timeLabel_ = new QLabel(QStringLiteral("--"));
        timeLabel_->setMinimumWidth(150);
        timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* timelineRow = new QWidget;
        auto* timelineLayout = new QHBoxLayout(timelineRow);
        timelineLayout->setContentsMargins(0, 0, 0, 0);
        timelineLayout->addWidget(timeline_, 1);
        timelineLayout->addWidget(timeLabel_);

        log_ = new QPlainTextEdit;
        log_->setReadOnly(true);
        log_->setMaximumBlockCount(4000);
        log_->setPlaceholderText(QStringLiteral("渲染日志会显示在这里"));

        auto* right = new QWidget;
        auto* rightLayout = new QVBoxLayout(right);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->addWidget(preview_, 4);
        rightLayout->addWidget(clipInfoLabel_);
        rightLayout->addWidget(timelineRow);
        rightLayout->addWidget(log_, 1);
        rightLayout->setContentsMargins(0, 0, 8, 8);

        connect(timeline_, &QSlider::valueChanged, this, [this](int value) {
            if (clipInfo_.fps > 0.0) {
                const QSignalBlocker blocker(previewTime_);
                previewTime_->setValue(value / clipInfo_.fps);
            }
            updateTimeLabel();
        });
        connect(timeline_, &QSlider::sliderReleased, this, [this] { previewFrame(); });
        connect(previewTime_, &QDoubleSpinBox::editingFinished, this,
                [this] { syncTimelineFromTime(); });
        connect(previewTime_, &QDoubleSpinBox::valueChanged, this,
                [this](double) { syncTimelineFromTime(); });

        auto* splitter = new QSplitter(Qt::Horizontal);
        splitter->addWidget(scroll);
        splitter->addWidget(right);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);
        splitter->setSizes({460, 860});

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(splitter);
    }

    // ---- settings --------------------------------------------------------

    void loadSettings() {
        QSettings settings(QStringLiteral("SpektraRoll"), QStringLiteral("SpektraRoll UI"));
        const QString defaultLut = repoRoot_ + QStringLiteral("/out/ui-portra400.cube");
        inputEdit_->setText(
            settings.value(QStringLiteral("paths/input"), defaultInput()).toString());
        ffmpegEdit_->setText(
            settings.value(QStringLiteral("paths/ffmpeg"), defaultFfmpeg()).toString());
        lutEdit_->setText(settings.value(QStringLiteral("paths/lut"), defaultLut).toString());
        outputEdit_->setText(settings
                                 .value(QStringLiteral("paths/output"),
                                        repoRoot_ + QStringLiteral("/out/ui-render.mov"))
                                 .toString());

        selectData(source_, settings.value(QStringLiteral("look/source"), QStringLiteral("apple-log")).toString());
        exposure_->setValue(settings.value(QStringLiteral("look/exposure"), 0.7).toDouble());
        selectData(scan_, settings.value(QStringLiteral("look/scan"), QStringLiteral("negative")).toString());
        contrast_->setValue(settings.value(QStringLiteral("look/contrast"), 1.15).toDouble());
        selectData(trc_, settings.value(QStringLiteral("look/trc"), QStringLiteral("srgb")).toString());

        grain_->setValue(settings.value(QStringLiteral("texture/grain"), 0.4).toDouble());
        grainRate_->setValue(settings.value(QStringLiteral("texture/grainRate"), 3).toInt());
        halation_->setValue(settings.value(QStringLiteral("texture/halation"), 0.45).toDouble());
        glare_->setValue(settings.value(QStringLiteral("texture/glare"), 0.2).toDouble());
        diffusion_->setValue(settings.value(QStringLiteral("texture/diffusion"), 0.1).toDouble());

        selectData(codec_, settings.value(QStringLiteral("render/codec"), QStringLiteral("h264")).toString());
        start_->setValue(settings.value(QStringLiteral("render/start"), 0.0).toDouble());
        frames_->setValue(settings.value(QStringLiteral("render/frames"), 0).toInt());
        segment_->setValue(settings.value(QStringLiteral("render/segment"), 10).toInt());
        resume_->setChecked(settings.value(QStringLiteral("render/resume"), true).toBool());
        previewTime_->setValue(settings.value(QStringLiteral("preview/time"), 1.5).toDouble());

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
        settings.setValue(QStringLiteral("look/exposure"), exposure_->value());
        settings.setValue(QStringLiteral("look/scan"), scan_->currentData().toString());
        settings.setValue(QStringLiteral("look/contrast"), contrast_->value());
        settings.setValue(QStringLiteral("look/trc"), trc_->currentData().toString());
        settings.setValue(QStringLiteral("texture/grain"), grain_->value());
        settings.setValue(QStringLiteral("texture/grainRate"), grainRate_->value());
        settings.setValue(QStringLiteral("texture/halation"), halation_->value());
        settings.setValue(QStringLiteral("texture/glare"), glare_->value());
        settings.setValue(QStringLiteral("texture/diffusion"), diffusion_->value());
        settings.setValue(QStringLiteral("render/codec"), codec_->currentData().toString());
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
        return true;
    }

    bool bakeLut() {
        QString path = lutEdit_->text().trimmed();
        if (path.isEmpty()) path = repoRoot_ + QStringLiteral("/out/ui-portra400.cube");
        QDir().mkpath(QFileInfo(path).absolutePath());

        QStringList args{QStringLiteral("lut"), QStringLiteral("bake"), QStringLiteral("--input"),
                         source_->currentData().toString(), QStringLiteral("--exposure"),
                         number(exposure_->value(), 2), QStringLiteral("--scan"),
                         scan_->currentData().toString(), QStringLiteral("--contrast"),
                         number(contrast_->value(), 2), QStringLiteral("--out"), path};
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
            QMessageBox::critical(this, QStringLiteral("烘焙失败"), output);
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
        status_->setText(QStringLiteral("LUT 已烘焙：") + QFileInfo(path).fileName());
        if (trc != QStringLiteral("srgb")) {
            appendLog(QStringLiteral("提示：HDR LUT 的画面在普通 SDR 屏上会偏暗，最终请用 HDR 显示器/iPhone 看。\n"));
        }
        return true;
    }

    bool ensureLut() {
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
        return QStringLiteral("%1|%2|%3|%4|%5")
            .arg(source_->currentData().toString())
            .arg(number(exposure_->value(), 4))
            .arg(scan_->currentData().toString())
            .arg(number(contrast_->value(), 4))
            .arg(trc_->currentData().toString());
    }

    QStringList textureArgs() const {
        QStringList args;
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
            clipInfoLabel_->setText(QStringLiteral("未选择素材（可以把视频拖进窗口）"));
            timeline_->setRange(0, 0);
            timeLabel_->setText(QStringLiteral("--"));
            return;
        }
        const ClipInfo info = probeClip(path);
        if (!info.valid()) {
            clipInfo_ = ClipInfo{};
            clipInfoLabel_->setText(QStringLiteral("读不到素材信息，检查 ffmpeg/ffprobe 路径"));
            timeline_->setRange(0, 0);
            timeLabel_->setText(QStringLiteral("--"));
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

        QDir().mkpath(repoRoot_ + QStringLiteral("/out"));
        const QString source = repoRoot_ + QStringLiteral("/out/ui-original-src.png");
        const QString output = repoRoot_ + QStringLiteral("/out/ui-original.png");
        const QString matrix = source_->currentData().toString() == QStringLiteral("apple-log")
                                   ? QStringLiteral("bt2020")
                                   : QStringLiteral("bt709");
        QStringList ffmpegArgs{
            QStringLiteral("-v"),          QStringLiteral("error"),
            QStringLiteral("-y"),          QStringLiteral("-ss"),
            number(time),                  QStringLiteral("-i"),
            inputEdit_->text(),            QStringLiteral("-frames:v"),
            QStringLiteral("1"),           QStringLiteral("-vf"),
            QStringLiteral("scale=in_color_matrix=") + matrix +
                QStringLiteral(":in_range=tv:out_range=full"),
            QStringLiteral("-pix_fmt"),    QStringLiteral("rgb48le"),
            source};

        QString outputText;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        bool ok = runSync(ffmpegEdit_->text(), ffmpegArgs, &outputText);
        if (ok) {
            QStringList args{QStringLiteral("convert"), QStringLiteral("--in"), source,
                             QStringLiteral("--out"), output};
            if (source_->currentData().toString() == QStringLiteral("apple-log")) {
                args << QStringLiteral("--from") << QStringLiteral("apple-log");
            }
            ok = runSync(cliPath_, args, &outputText);
        }
        QApplication::restoreOverrideCursor();

        if (!ok) {
            appendLog(outputText);
            status_->setText(QStringLiteral("原片预览失败，日志里有原因"));
            return;
        }
        const QImage image(output);
        if (image.isNull()) {
            status_->setText(QStringLiteral("原片读取失败"));
            return;
        }
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

    void previewFrame() {
        if (!requireInput() || !ensureLut()) return;

        QDir().mkpath(repoRoot_ + QStringLiteral("/out"));
        const QString source = repoRoot_ + QStringLiteral("/out/ui-preview-src.png");
        const QString output = repoRoot_ + QStringLiteral("/out/ui-preview.png");
        const QString matrix =
            source_->currentData().toString() == QStringLiteral("apple-log") ? QStringLiteral("bt2020")
                                                                             : QStringLiteral("bt709");

        QStringList ffmpegArgs{
            QStringLiteral("-v"),           QStringLiteral("error"),
            QStringLiteral("-y"),           QStringLiteral("-ss"),
            number(previewTime_->value()),  QStringLiteral("-i"),
            inputEdit_->text(),             QStringLiteral("-frames:v"),
            QStringLiteral("1"),            QStringLiteral("-vf"),
            QStringLiteral("scale=in_color_matrix=") + matrix +
                QStringLiteral(":in_range=tv:out_range=full"),
            QStringLiteral("-pix_fmt"),     QStringLiteral("rgb48le"),
            source};

        QString outputText;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        bool ok = runSync(ffmpegEdit_->text(), ffmpegArgs, &outputText);
        if (ok) {
            QStringList args{QStringLiteral("lut"), QStringLiteral("apply"),
                             QStringLiteral("--lut"), lutEdit_->text(),
                             QStringLiteral("--in"),  source,
                             QStringLiteral("--out"), output};
            args << textureArgs();
            ok = runSync(cliPath_, args, &outputText);
        }
        QApplication::restoreOverrideCursor();

        if (!ok) {
            appendLog(outputText);
            status_->setText(QStringLiteral("预览失败，日志里有原因"));
            return;
        }
        const QImage image(output);
        if (image.isNull()) {
            status_->setText(QStringLiteral("预览图读取失败"));
            return;
        }
        preview_->setImage(image);
        gradedImage_ = image;
        originalImage_ = QImage();
        originalTime_ = -1.0;
        status_->setText(QStringLiteral("预览：%1 × %2 @ %3 s")
                             .arg(image.width())
                             .arg(image.height())
                             .arg(number(previewTime_->value(), 2)));
    }

    void startRender() {
        if (!requireInput() || !ensureLut() || rendering_) return;

        const QString output = outputEdit_->text().trimmed();
        if (output.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("缺少输出"),
                                 QStringLiteral("请指定输出文件。"));
            return;
        }
        QDir().mkpath(QFileInfo(output).absolutePath());

        QStringList args{QStringLiteral("video"), QStringLiteral("--lut"), lutEdit_->text(),
                         QStringLiteral("--in"), inputEdit_->text(), QStringLiteral("--out"),
                         output, QStringLiteral("--trc"), trc_->currentData().toString(),
                         QStringLiteral("--ffmpeg"), ffmpegEdit_->text()};
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

        log_->clear();
        appendLog(QStringLiteral("> ") + cliPath_ + QLatin1Char(' ') + args.join(QLatin1Char(' ')) +
                  QLatin1Char('\n'));
        pending_.clear();
        canceled_ = false;
        rendering_ = true;
        setBusy(true);
        progress_->setRange(0, 0);
        status_->setText(QStringLiteral("渲染中…"));

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
                        const auto answer = QMessageBox::question(
                            this, QStringLiteral("完成"),
                            QStringLiteral("渲染完成。现在打开文件所在文件夹吗？"));
                        if (answer == QMessageBox::Yes) {
                            QDesktopServices::openUrl(
                                QUrl::fromLocalFile(QFileInfo(outputEdit_->text()).absolutePath()));
                        }
                    } else {
                        status_->setText(QStringLiteral("渲染失败，日志里有原因"));
                        progress_->setRange(0, 100);
                    }
                });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
            appendLog(QStringLiteral("进程错误：") + process->errorString() + QLatin1Char('\n'));
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
            status_->setText(line);
            const auto match = framesRe.match(line);
            if (match.hasMatch()) {
                const int done = match.captured(1).toInt();
                const int total = match.captured(2).toInt();
                progress_->setRange(0, total);
                progress_->setValue(done);
            }
        }
    }

    void setBusy(bool busy) {
        bakeButton_->setEnabled(!busy);
        previewButton_->setEnabled(!busy);
        renderButton_->setEnabled(!busy);
        cancelButton_->setEnabled(busy);
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

    // ---- state -----------------------------------------------------------

    QString repoRoot_;
    QString cliPath_;

    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* ffmpegEdit_ = nullptr;
    QLineEdit* lutEdit_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;

    QComboBox* source_ = nullptr;
    QDoubleSpinBox* exposure_ = nullptr;
    QComboBox* scan_ = nullptr;
    QDoubleSpinBox* contrast_ = nullptr;
    QComboBox* trc_ = nullptr;

    QDoubleSpinBox* grain_ = nullptr;
    QSpinBox* grainRate_ = nullptr;
    QDoubleSpinBox* halation_ = nullptr;
    QDoubleSpinBox* glare_ = nullptr;
    QDoubleSpinBox* diffusion_ = nullptr;

    QComboBox* codec_ = nullptr;
    QDoubleSpinBox* start_ = nullptr;
    QSpinBox* frames_ = nullptr;
    QSpinBox* segment_ = nullptr;
    QCheckBox* resume_ = nullptr;
    QDoubleSpinBox* previewTime_ = nullptr;

    QPushButton* bakeButton_ = nullptr;
    QPushButton* previewButton_ = nullptr;
    QPushButton* compareButton_ = nullptr;
    QPushButton* renderButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;

    QProgressBar* progress_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* clipInfoLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QSlider* timeline_ = nullptr;
    PreviewView* preview_ = nullptr;

    ClipInfo clipInfo_;
    QImage gradedImage_;
    QImage originalImage_;
    double originalTime_ = -1.0;
    QString lastProbePath_;

    QProcess* renderProcess_ = nullptr;
    QString pending_;
    bool rendering_ = false;
    bool canceled_ = false;
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
    MainWindow window;
    window.show();
    return app.exec();
}
