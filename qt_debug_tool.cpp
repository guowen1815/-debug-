#include <QtWidgets>

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>

constexpr int kReplayMinutes = 60;
constexpr int kMaxVisibleCities = 10;

struct CaseConfig {
    int M = 0;
    int N = 0;
    int R = 0;
    int K = 0;
    int T = 0;
};

struct LineInfo {
    int globalLine = 0;
    int caseNo = 0;
    int caseLine = 0;
    int minute = -1;
    int city = -1;
    bool redHeadquarter = false;
    bool blueHeadquarter = false;
    QVector<int> relatedCities;
    QString text;
};

struct CompareResult {
    bool same = true;
    int index = -1;
    QString expected;
    QString actual;
    bool expectedMissing = false;
    bool actualMissing = false;
};

struct ReplayEvent {
    LineInfo info;
    QString actualText;
    bool differs = false;
};

struct WarriorSnapshot {
    QString owner;
    QString type;
    int id = 0;
    int city = -1;
    bool redHeadquarter = false;
    bool blueHeadquarter = false;
    int hp = -1;
    int force = -1;
    QString weapon;
    QString line;
    bool differs = false;
};

struct AnalysisReport {
    bool valid = false;
    bool same = true;
    QString error;
    CompareResult diff;
    QVector<CaseConfig> cases;
    QVector<QString> expectedLines;
    QVector<QString> actualLines;
    QVector<LineInfo> expectedInfo;
    QVector<ReplayEvent> replayEvents;
    QVector<WarriorSnapshot> snapshots;
    QSet<int> focusCities;
    QVector<int> visibleCities;
    int mismatchMinute = -1;
    int replayStartMinute = -1;
    QString redHeadquarterStatus;
    QString blueHeadquarterStatus;
};

static QStringList readLines(const QString& path, bool* ok = nullptr) {
    QFile file(path);
    QStringList lines;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok) *ok = false;
        return lines;
    }
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.endsWith('\r')) line.chop(1);
        lines.push_back(line);
    }
    if (ok) *ok = true;
    return lines;
}

static QVector<QString> toVector(const QStringList& lines) {
    QVector<QString> out;
    out.reserve(lines.size());
    for (const QString& line : lines) out.push_back(line);
    return out;
}

static bool readCases(const QString& path, QVector<CaseConfig>* cases, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = "无法读取输入文件: " + path;
        return false;
    }
    QTextStream in(&file);
    int tc = 0;
    in >> tc;
    if (in.status() != QTextStream::Ok || tc <= 0) {
        if (error) *error = "输入文件格式不正确，无法读取测试用例数量。";
        return false;
    }
    cases->clear();
    cases->resize(tc);
    for (int i = 0; i < tc; ++i) {
        CaseConfig cfg;
        int hp[5], atk[5];
        in >> cfg.M >> cfg.N >> cfg.R >> cfg.K >> cfg.T;
        for (int j = 0; j < 5; ++j) in >> hp[j];
        for (int j = 0; j < 5; ++j) in >> atk[j];
        if (in.status() != QTextStream::Ok) {
            if (error) *error = QString("输入文件第 %1 个测试用例不完整。").arg(i + 1);
            return false;
        }
        (*cases)[i] = cfg;
    }
    return true;
}

static int parseMinute(const QString& line) {
    if (line.size() < 6) return -1;
    if (!line[0].isDigit() || !line[1].isDigit() || !line[2].isDigit() ||
        line[3] != ':' || !line[4].isDigit() || !line[5].isDigit()) {
        return -1;
    }
    int hour = line.mid(0, 3).toInt();
    int minute = line.mid(4, 2).toInt();
    return hour * 60 + minute;
}

static QString minuteToString(int minute) {
    if (minute < 0) return "unknown";
    return QString("%1:%2")
        .arg(minute / 60, 3, 10, QLatin1Char('0'))
        .arg(minute % 60, 2, 10, QLatin1Char('0'));
}

static int clampInt(int value, int low, int high) {
    return std::max(low, std::min(value, high));
}

static void addCityWindow(QSet<int>* focus, int n, int requestedStart) {
    if (n <= 0) return;
    int count = std::min(n, kMaxVisibleCities);
    int start = clampInt(requestedStart, 1, n - count + 1);
    for (int city = start; city < start + count; ++city) focus->insert(city);
}

static QVector<int> extractCities(const QString& line) {
    QVector<int> cities;
    static const QRegularExpression cityPattern(R"(city (\d+))");
    QRegularExpressionMatchIterator it = cityPattern.globalMatch(line);
    while (it.hasNext()) cities.push_back(it.next().captured(1).toInt());
    std::sort(cities.begin(), cities.end());
    cities.erase(std::unique(cities.begin(), cities.end()), cities.end());
    return cities;
}

static QVector<LineInfo> buildLineInfo(const QVector<QString>& lines) {
    QVector<LineInfo> infos;
    infos.reserve(lines.size());
    int caseNo = 0;
    int caseLine = 0;
    for (int i = 0; i < lines.size(); ++i) {
        LineInfo info;
        info.globalLine = i + 1;
        info.text = lines[i];
        if (lines[i].startsWith("Case ")) {
            ++caseNo;
            caseLine = 1;
            info.caseNo = caseNo;
            info.caseLine = caseLine;
            infos.push_back(info);
            continue;
        }
        if (caseNo == 0) caseNo = 1;
        info.caseNo = caseNo;
        info.caseLine = ++caseLine;
        info.minute = parseMinute(lines[i]);
        info.relatedCities = extractCities(lines[i]);
        if (!info.relatedCities.empty()) info.city = info.relatedCities.front();
        info.redHeadquarter = lines[i].contains("red headquarter");
        info.blueHeadquarter = lines[i].contains("blue headquarter");
        infos.push_back(info);
    }
    return infos;
}

static CompareResult compareLines(const QVector<QString>& expected, const QVector<QString>& actual) {
    CompareResult result;
    int common = std::min(expected.size(), actual.size());
    for (int i = 0; i < common; ++i) {
        if (expected[i] != actual[i]) {
            result.same = false;
            result.index = i;
            result.expected = expected[i];
            result.actual = actual[i];
            return result;
        }
    }
    if (expected.size() != actual.size()) {
        result.same = false;
        result.index = common;
        result.expectedMissing = expected.size() <= common;
        result.actualMissing = actual.size() <= common;
        if (!result.expectedMissing) result.expected = expected[common];
        if (!result.actualMissing) result.actual = actual[common];
    }
    return result;
}

static QSet<int> buildFocusCities(const LineInfo& mismatch, const QVector<CaseConfig>& cases) {
    QSet<int> focus;
    if (mismatch.caseNo < 1 || mismatch.caseNo > cases.size()) return focus;
    int n = cases[mismatch.caseNo - 1].N;
    if (!mismatch.relatedCities.empty()) {
        addCityWindow(&focus, n, mismatch.relatedCities.front() - kMaxVisibleCities / 2);
        return focus;
    }
    if (mismatch.redHeadquarter) {
        addCityWindow(&focus, n, 1);
    } else if (mismatch.blueHeadquarter) {
        addCityWindow(&focus, n, n - kMaxVisibleCities + 1);
    } else {
        addCityWindow(&focus, n, 1);
    }
    return focus;
}

static bool touchesFocus(const LineInfo& info, const QSet<int>& focus) {
    if (focus.empty()) return true;
    for (int city : info.relatedCities) {
        if (focus.contains(city)) return true;
    }
    if (info.redHeadquarter && (focus.contains(1) || focus.contains(2))) return true;
    if (info.blueHeadquarter && !focus.empty()) {
        QList<int> cities = focus.values();
        std::sort(cities.begin(), cities.end());
        int maxCity = cities.back();
        if (focus.contains(maxCity) || focus.contains(maxCity - 1)) return true;
    }
    return false;
}

static bool extractWarrior(const QString& line, WarriorSnapshot* snapshot) {
    static const QRegularExpression warriorPattern(
        R"((red|blue) (dragon|ninja|iceman|lion|wolf) (\d+))");
    QRegularExpressionMatch m = warriorPattern.match(line);
    if (!m.hasMatch()) return false;
    snapshot->owner = m.captured(1);
    snapshot->type = m.captured(2);
    snapshot->id = m.captured(3).toInt();
    snapshot->line = line;
    snapshot->city = -1;
    snapshot->redHeadquarter = line.contains("red headquarter");
    snapshot->blueHeadquarter = line.contains("blue headquarter");

    static const QRegularExpression cityPattern(R"(city (\d+))");
    QRegularExpressionMatch cityMatch = cityPattern.match(line);
    if (cityMatch.hasMatch()) snapshot->city = cityMatch.captured(1).toInt();

    static const QRegularExpression hpForcePattern(R"(with (\d+) elements and force (\d+))");
    QRegularExpressionMatch hpForce = hpForcePattern.match(line);
    if (hpForce.hasMatch()) {
        snapshot->hp = hpForce.captured(1).toInt();
        snapshot->force = hpForce.captured(2).toInt();
    }

    static const QRegularExpression weaponPattern(R"( has (.+)$)");
    QRegularExpressionMatch weapon = weaponPattern.match(line);
    if (weapon.hasMatch()) snapshot->weapon = weapon.captured(1);
    return true;
}

static QVector<WarriorSnapshot> buildSnapshots(const QVector<ReplayEvent>& events) {
    QMap<QString, WarriorSnapshot> latest;
    for (const ReplayEvent& event : events) {
        WarriorSnapshot snapshot;
        if (!extractWarrior(event.info.text, &snapshot)) continue;
        snapshot.differs = event.differs;
        QString key = snapshot.owner + ":" + QString::number(snapshot.id);
        if (latest.contains(key)) {
            WarriorSnapshot merged = latest[key];
            if (snapshot.city != -1 || snapshot.redHeadquarter || snapshot.blueHeadquarter) {
                merged.city = snapshot.city;
                merged.redHeadquarter = snapshot.redHeadquarter;
                merged.blueHeadquarter = snapshot.blueHeadquarter;
            }
            if (snapshot.hp != -1) merged.hp = snapshot.hp;
            if (snapshot.force != -1) merged.force = snapshot.force;
            if (!snapshot.weapon.isEmpty()) merged.weapon = snapshot.weapon;
            merged.line = snapshot.line;
            merged.differs = merged.differs || snapshot.differs;
            latest[key] = merged;
        } else {
            latest[key] = snapshot;
        }
    }
    return latest.values().toVector();
}

static QString lastHeadquarterStatus(const QVector<LineInfo>& infos, int caseNo, int minute, bool red) {
    QString last = red ? "未找到红司令部生命元记录。" : "未找到蓝司令部生命元记录。";
    QString needle = red ? "elements in red headquarter" : "elements in blue headquarter";
    for (const LineInfo& info : infos) {
        if (info.caseNo != caseNo || info.minute < 0 || info.minute > minute) continue;
        if (info.text.contains(needle)) last = info.text;
    }
    return last;
}

static AnalysisReport analyzeFiles(const QString& inputPath,
                                   const QString& expectedPath,
                                   const QString& actualPath) {
    AnalysisReport report;
    QString error;
    if (!readCases(inputPath, &report.cases, &error)) {
        report.error = error;
        return report;
    }

    bool expectedOk = false;
    bool actualOk = false;
    report.expectedLines = toVector(readLines(expectedPath, &expectedOk));
    report.actualLines = toVector(readLines(actualPath, &actualOk));
    if (!expectedOk) {
        report.error = "无法读取标准输出文件: " + expectedPath;
        return report;
    }
    if (!actualOk) {
        report.error = "无法读取实际输出文件: " + actualPath;
        return report;
    }

    report.expectedInfo = buildLineInfo(report.expectedLines);
    report.diff = compareLines(report.expectedLines, report.actualLines);
    report.same = report.diff.same;
    report.valid = true;
    if (report.same) return report;

    if (report.diff.index < 0 || report.diff.index >= report.expectedInfo.size()) return report;
    const LineInfo& mismatch = report.expectedInfo[report.diff.index];
    report.mismatchMinute = mismatch.minute;
    report.focusCities = buildFocusCities(mismatch, report.cases);
    QList<int> focusList = report.focusCities.values();
    std::sort(focusList.begin(), focusList.end());
    for (int city : focusList) report.visibleCities.push_back(city);
    report.replayStartMinute = std::max(0, mismatch.minute - kReplayMinutes);
    report.redHeadquarterStatus = lastHeadquarterStatus(report.expectedInfo, mismatch.caseNo, mismatch.minute, true);
    report.blueHeadquarterStatus = lastHeadquarterStatus(report.expectedInfo, mismatch.caseNo, mismatch.minute, false);

    for (const LineInfo& info : report.expectedInfo) {
        if (info.caseNo != mismatch.caseNo || info.minute < report.replayStartMinute ||
            info.minute > mismatch.minute) {
            continue;
        }
        bool hqEvent = info.text.contains("elements in red headquarter") ||
                       info.text.contains("elements in blue headquarter") ||
                       info.text.contains("reached red headquarter") ||
                       info.text.contains("reached blue headquarter") ||
                       info.text.contains("headquarter was taken");
        if (!touchesFocus(info, report.focusCities) && !hqEvent) continue;

        ReplayEvent event;
        event.info = info;
        if (info.globalLine - 1 < report.actualLines.size()) {
            event.actualText = report.actualLines[info.globalLine - 1];
            event.differs = event.actualText != info.text;
        } else {
            event.actualText = "<actual output ended>";
            event.differs = true;
        }
        report.replayEvents.push_back(event);
    }
    report.snapshots = buildSnapshots(report.replayEvents);
    return report;
}

class LocationItem : public QGraphicsObject {
    Q_OBJECT

public:
    LocationItem(const QString& key, const QString& title, const QColor& color)
        : key_(key), title_(title), color_(color) {
        setAcceptHoverEvents(true);
        setFlag(QGraphicsItem::ItemIsSelectable);
    }

    QRectF boundingRect() const override { return QRectF(0, 0, 104, 78); }

    void setSubtitle(const QString& subtitle) {
        subtitle_ = subtitle;
        update();
    }

    void setWarning(bool warning) {
        warning_ = warning;
        update();
    }

    QString key() const { return key_; }
    void setKey(const QString& key) { key_ = key; }

signals:
    void selected(const QString& key);

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        QColor fill = isSelected() ? color_.lighter(115) : color_;
        painter->setPen(QPen(warning_ ? QColor("#c62828") : QColor("#263238"), warning_ ? 3 : 1));
        painter->setBrush(fill);
        painter->drawRoundedRect(boundingRect().adjusted(1, 1, -1, -1), 8, 8);

        painter->setPen(QColor("#111827"));
        QFont titleFont = painter->font();
        titleFont.setBold(true);
        titleFont.setPointSize(10);
        painter->setFont(titleFont);
        painter->drawText(QRectF(6, 9, 92, 24), Qt::AlignCenter, title_);

        QFont subFont = painter->font();
        subFont.setBold(false);
        subFont.setPointSize(9);
        painter->setFont(subFont);
        painter->setPen(warning_ ? QColor("#b71c1c") : QColor("#374151"));
        painter->drawText(QRectF(6, 36, 92, 34), Qt::AlignCenter | Qt::TextWordWrap, subtitle_);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        QGraphicsObject::mousePressEvent(event);
        setSelected(true);
        emit selected(key_);
    }

private:
    QString key_;
    QString title_;
    QString subtitle_;
    QColor color_;
    bool warning_ = false;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow() {
        setWindowTitle("魔兽作业 Debug Tool");
        resize(1180, 760);
        buildUi();
        setDefaultPaths();
        updateMode();
    }

private slots:
    void browseProgram() { pickFile(programEdit_, "选择学生程序或 C++ 源文件", "C++ files (*.cpp *.cc *.cxx);;Executables (*)"); }
    void browseInput() { pickFile(inputEdit_, "选择输入数据", "Text files (*.in *.txt);;All files (*)"); }
    void browseExpected() { pickFile(expectedEdit_, "选择标准输出", "Output files (*.out *.txt);;All files (*)"); }
    void browseActual() { pickFile(actualEdit_, "选择实际输出", "Output files (*.out *.txt);;All files (*)"); }

    void updateMode() {
        bool runMode = modeBox_->currentIndex() == 0;
        programEdit_->setEnabled(runMode);
        programButton_->setEnabled(runMode);
        runButton_->setText(runMode ? "编译/运行并比对" : "比对已有输出");
    }

    void runAnalysis() {
        statusBar()->showMessage("正在分析...");
        QString actualPath = actualEdit_->text().trimmed();
        if (modeBox_->currentIndex() == 0) {
            QString program = programEdit_->text().trimmed();
            if (program.isEmpty()) {
                showError("请先选择学生程序。");
                return;
            }
            QString runnable = program;
            if (program.endsWith(".cpp", Qt::CaseInsensitive) ||
                program.endsWith(".cc", Qt::CaseInsensitive) ||
                program.endsWith(".cxx", Qt::CaseInsensitive)) {
                runnable = QDir::temp().filePath(QString("moshou_debug_tool_target_%1").arg(QCoreApplication::applicationPid()));
                QProcess compiler;
                compiler.setProgram("clang++");
                compiler.setArguments({"-std=c++17", "-O2", program, "-o", runnable});
                compiler.start();
                if (!compiler.waitForStarted(3000)) {
                    showError("编译器启动失败: " + compiler.errorString());
                    return;
                }
                if (!compiler.waitForFinished(30000) || compiler.exitCode() != 0) {
                    QString compileLog = QString::fromLocal8Bit(compiler.readAllStandardError());
                    showError("C++ 源文件编译失败:\n" + compileLog.left(4000));
                    return;
                }
            }
            QProcess process;
            process.setProgram(runnable);
            process.setStandardInputFile(inputEdit_->text().trimmed());
            process.setStandardOutputFile(actualPath);
            process.start();
            if (!process.waitForStarted(3000)) {
                showError("学生程序启动失败: " + process.errorString());
                return;
            }
            if (!process.waitForFinished(30000)) {
                process.kill();
                showError("学生程序运行超过 30 秒，已中止。");
                return;
            }
            exitCodeLabel_->setText(QString("Program exit code: %1").arg(process.exitCode()));
        } else {
            exitCodeLabel_->setText("Compare-only mode");
        }

        report_ = analyzeFiles(inputEdit_->text().trimmed(), expectedEdit_->text().trimmed(), actualPath);
        if (!report_.valid) {
            showError(report_.error);
            return;
        }
        populateReport();
        statusBar()->showMessage("分析完成", 4000);
    }

    void selectLocation(const QString& key) {
        for (LocationItem* item : locationItems_) item->setSelected(item->key() == key);
        populateDetail(key);
    }

private:
    void buildUi() {
        QWidget* central = new QWidget;
        QVBoxLayout* root = new QVBoxLayout(central);
        root->setContentsMargins(14, 12, 14, 12);
        root->setSpacing(10);

        QGridLayout* form = new QGridLayout;
        form->setHorizontalSpacing(8);
        form->setVerticalSpacing(8);
        modeBox_ = new QComboBox;
        modeBox_->addItems({"运行学生程序/源码", "比对已有输出"});
        connect(modeBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateMode);
        form->addWidget(new QLabel("模式"), 0, 0);
        form->addWidget(modeBox_, 0, 1);

        programEdit_ = new QLineEdit;
        programButton_ = new QPushButton("浏览");
        connect(programButton_, &QPushButton::clicked, this, &MainWindow::browseProgram);
        form->addWidget(new QLabel("学生程序"), 0, 2);
        form->addWidget(programEdit_, 0, 3);
        form->addWidget(programButton_, 0, 4);

        inputEdit_ = addPathRow(form, 1, "输入数据", &MainWindow::browseInput);
        expectedEdit_ = addPathRow(form, 2, "标准输出", &MainWindow::browseExpected);
        actualEdit_ = addPathRow(form, 3, "实际输出", &MainWindow::browseActual);

        runButton_ = new QPushButton("编译/运行并比对");
        runButton_->setMinimumHeight(34);
        connect(runButton_, &QPushButton::clicked, this, &MainWindow::runAnalysis);
        form->addWidget(runButton_, 3, 4);
        form->setColumnStretch(3, 1);
        root->addLayout(form);

        QSplitter* splitter = new QSplitter(Qt::Horizontal);
        QWidget* left = new QWidget;
        QVBoxLayout* leftLayout = new QVBoxLayout(left);
        leftLayout->setContentsMargins(0, 0, 0, 0);

        summaryBox_ = new QTextEdit;
        summaryBox_->setReadOnly(true);
        summaryBox_->setMinimumHeight(150);
        leftLayout->addWidget(summaryBox_);

        QHBoxLayout* cityWindowLayout = new QHBoxLayout;
        cityWindowLayout->setContentsMargins(0, 0, 0, 0);
        cityWindowLabel_ = new QLabel("城市窗口: -");
        citySlider_ = new QSlider(Qt::Horizontal);
        citySlider_->setMinimum(1);
        citySlider_->setMaximum(1);
        citySlider_->setValue(1);
        citySlider_->setEnabled(false);
        connect(citySlider_, &QSlider::valueChanged, this, [this](int start) {
            updateVisibleCitiesFromStart(start);
            populateMap();
        });
        cityWindowLayout->addWidget(cityWindowLabel_);
        cityWindowLayout->addWidget(citySlider_, 1);
        leftLayout->addLayout(cityWindowLayout);

        scene_ = new QGraphicsScene(this);
        view_ = new QGraphicsView(scene_);
        view_->setRenderHint(QPainter::Antialiasing);
        view_->setMinimumHeight(170);
        leftLayout->addWidget(view_);
        createMapItems();

        eventTable_ = new QTableWidget(0, 4);
        eventTable_->setHorizontalHeaderLabels({"行号", "时间", "标准事件", "实际事件"});
        eventTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        eventTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        eventTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        eventTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        eventTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        leftLayout->addWidget(eventTable_, 1);
        splitter->addWidget(left);

        QWidget* right = new QWidget;
        QVBoxLayout* rightLayout = new QVBoxLayout(right);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        locationTitle_ = new QLabel("点击司令部或城市查看详情");
        QFont titleFont = locationTitle_->font();
        titleFont.setBold(true);
        titleFont.setPointSize(12);
        locationTitle_->setFont(titleFont);
        rightLayout->addWidget(locationTitle_);

        warriorTable_ = new QTableWidget(0, 6);
        warriorTable_->setHorizontalHeaderLabels({"阵营", "武士", "编号", "生命", "攻击", "武器/最近事件"});
        warriorTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        warriorTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
        rightLayout->addWidget(warriorTable_, 1);

        detailBox_ = new QTextEdit;
        detailBox_->setReadOnly(true);
        detailBox_->setMinimumHeight(220);
        rightLayout->addWidget(detailBox_);
        splitter->addWidget(right);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        root->addWidget(splitter, 1);

        exitCodeLabel_ = new QLabel;
        statusBar()->addPermanentWidget(exitCodeLabel_);
        setCentralWidget(central);
    }

    QLineEdit* addPathRow(QGridLayout* form, int row, const QString& label, void (MainWindow::*slot)()) {
        QLineEdit* edit = new QLineEdit;
        QPushButton* button = new QPushButton("浏览");
        connect(button, &QPushButton::clicked, this, slot);
        form->addWidget(new QLabel(label), row, 0);
        form->addWidget(edit, row, 1, 1, 3);
        form->addWidget(button, row, 4);
        return edit;
    }

    void createMapItems() {
        scene_->clear();
        locationItems_.clear();
        QVector<QPair<QString, QString>> nodes;
        nodes.push_back({"red_hq", "红司令部"});
        for (int i = 1; i <= kMaxVisibleCities; ++i) {
            nodes.push_back({QString("city_none_%1").arg(i), QString("城市 %1").arg(i)});
        }
        nodes.push_back({"blue_hq", "蓝司令部"});
        for (int i = 0; i < nodes.size(); ++i) {
            QColor color = i == 0 ? QColor("#ffcdd2") : (i == nodes.size() - 1 ? QColor("#bbdefb") : QColor("#e8f5e9"));
            LocationItem* item = new LocationItem(nodes[i].first, nodes[i].second, color);
            item->setPos(14 + i * 112, 36);
            connect(item, &LocationItem::selected, this, &MainWindow::selectLocation);
            scene_->addItem(item);
            locationItems_.push_back(item);
        }
        scene_->setSceneRect(0, 0, 14 + nodes.size() * 112, 150);
    }

    void setDefaultPaths() {
        QString base = QCoreApplication::applicationDirPath();
        if (!QFile::exists(base + "/data.in")) {
            base = QDir::currentPath();
        }
        inputEdit_->setText(QDir(base).filePath("data.in"));
        expectedEdit_->setText(QDir(base).filePath("data.out"));
        actualEdit_->setText(QDir(base).filePath("actual.out"));
        QString mainExe = QDir(base).filePath("main");
        if (QFile::exists(mainExe)) programEdit_->setText(mainExe);
        QString sampleCpp = QDir(base).filePath("some_cpp_file_with_bug/actual.cpp");
        if (QFile::exists(sampleCpp)) programEdit_->setText(sampleCpp);
    }

    void pickFile(QLineEdit* edit, const QString& title, const QString& filter) {
        QString path = QFileDialog::getOpenFileName(this, title, edit->text(), filter.isEmpty() ? "All files (*)" : filter);
        if (!path.isEmpty()) edit->setText(path);
    }

    void showError(const QString& text) {
        statusBar()->showMessage("分析失败", 4000);
        QMessageBox::warning(this, "Debug Tool", text);
    }

    void populateReport() {
        eventTable_->setRowCount(0);
        warriorTable_->setRowCount(0);
        detailBox_->clear();

        if (report_.same) {
            summaryBox_->setPlainText(QString("输出完全一致。\nTotal lines checked: %1")
                                      .arg(report_.expectedLines.size()));
            cityWindowLabel_->setText("城市窗口: 无差异");
            citySlider_->setEnabled(false);
            for (LocationItem* item : locationItems_) {
                item->setSubtitle("无差异");
                item->setWarning(false);
            }
            return;
        }

        const LineInfo& mismatch = report_.expectedInfo[report_.diff.index];
        QStringList focus;
        QList<int> focusList = report_.focusCities.values();
        std::sort(focusList.begin(), focusList.end());
        for (int city : focusList) focus << QString::number(city);

        QString summary;
        QTextStream out(&summary);
        out << "首个差异: data.out 第 " << (report_.diff.index + 1) << " 行\n";
        out << "Case: " << mismatch.caseNo << "    Case line: " << mismatch.caseLine;
        if (mismatch.minute >= 0) out << "    Time: " << minuteToString(mismatch.minute);
        out << "\n";
        out << "Expected: " << (report_.diff.expectedMissing ? "<expected output ended>" : report_.diff.expected) << "\n";
        out << "Actual  : " << (report_.diff.actualMissing ? "<actual output ended>" : report_.diff.actual) << "\n";
        out << "Replay : " << minuteToString(report_.replayStartMinute) << " -> " << minuteToString(report_.mismatchMinute) << "\n";
        out << "Focus  : " << (focus.isEmpty() ? "司令部/全局事件" : "city " + focus.join(", city ")) << "\n";
        out << report_.redHeadquarterStatus << "\n" << report_.blueHeadquarterStatus;
        summaryBox_->setPlainText(summary);

        initializeCityWindow();
        populateMap();
        populateEvents();
        if (locationItems_.size() > 1) selectLocation(locationItems_[1]->key());
    }

    void populateMap() {
        for (int i = 0; i < locationItems_.size(); ++i) {
            LocationItem* item = locationItems_[i];
            item->setWarning(false);
            item->setVisible(true);
            if (i == 0) {
                item->setKey("red_hq");
                item->setSubtitle(shortHeadquarter(report_.redHeadquarterStatus));
                item->setWarning(hasDiffForLocation("red_hq"));
            } else if (i == locationItems_.size() - 1) {
                item->setKey("blue_hq");
                item->setSubtitle(shortHeadquarter(report_.blueHeadquarterStatus));
                item->setWarning(hasDiffForLocation("blue_hq"));
            } else {
                int city = i - 1 < report_.visibleCities.size() ? report_.visibleCities[i - 1] : -1;
                item->setKey(city > 0 ? QString("city_%1").arg(city) : QString("city_none_%1").arg(i));
                item->setSubtitle(city > 0 ? QString("city %1").arg(city) : "未显示");
                item->setWarning(city > 0 && hasDiffForLocation(QString("city_%1").arg(city)));
                item->setVisible(city > 0);
            }
        }
    }

    QString shortHeadquarter(const QString& status) const {
        static const QRegularExpression numberPattern(R"((\d+) elements)");
        QRegularExpressionMatch m = numberPattern.match(status);
        return m.hasMatch() ? m.captured(1) + " elements" : status;
    }

    int currentCaseCityCount() const {
        if (!report_.valid || report_.same || report_.diff.index < 0 ||
            report_.diff.index >= report_.expectedInfo.size()) {
            return 0;
        }
        int caseNo = report_.expectedInfo[report_.diff.index].caseNo;
        if (caseNo < 1 || caseNo > report_.cases.size()) return 0;
        return report_.cases[caseNo - 1].N;
    }

    void updateVisibleCitiesFromStart(int start) {
        report_.visibleCities.clear();
        int n = currentCaseCityCount();
        if (n <= 0) {
            cityWindowLabel_->setText("城市窗口: -");
            citySlider_->setEnabled(false);
            return;
        }
        int count = std::min(n, kMaxVisibleCities);
        start = clampInt(start, 1, n - count + 1);
        for (int city = start; city < start + count; ++city) report_.visibleCities.push_back(city);
        citySlider_->blockSignals(true);
        citySlider_->setMinimum(1);
        citySlider_->setMaximum(std::max(1, n - count + 1));
        citySlider_->setValue(start);
        citySlider_->setEnabled(n > kMaxVisibleCities);
        citySlider_->blockSignals(false);
        cityWindowLabel_->setText(QString("城市窗口: %1-%2 / N=%3")
                                  .arg(start)
                                  .arg(start + count - 1)
                                  .arg(n));
    }

    void initializeCityWindow() {
        int n = currentCaseCityCount();
        if (n <= 0) {
            updateVisibleCitiesFromStart(1);
            return;
        }
        int start = 1;
        if (!report_.visibleCities.empty()) {
            start = report_.visibleCities.front();
        }
        updateVisibleCitiesFromStart(start);
    }

    bool hasDiffForLocation(const QString& key) const {
        for (const ReplayEvent& event : report_.replayEvents) {
            if (!event.differs) continue;
            if (key == "red_hq" && event.info.redHeadquarter) return true;
            if (key == "blue_hq" && event.info.blueHeadquarter) return true;
            if (key.startsWith("city_")) {
                int city = key.mid(5).toInt();
                if (event.info.relatedCities.contains(city)) return true;
            }
        }
        return false;
    }

    void populateEvents() {
        eventTable_->setRowCount(report_.replayEvents.size());
        for (int row = 0; row < report_.replayEvents.size(); ++row) {
            const ReplayEvent& event = report_.replayEvents[row];
            QList<QTableWidgetItem*> items = {
                new QTableWidgetItem(QString::number(event.info.globalLine)),
                new QTableWidgetItem(minuteToString(event.info.minute)),
                new QTableWidgetItem(event.info.text),
                new QTableWidgetItem(event.actualText)
            };
            for (int col = 0; col < items.size(); ++col) {
                if (event.differs) items[col]->setBackground(QColor("#ffebee"));
                eventTable_->setItem(row, col, items[col]);
            }
        }
    }

    void populateDetail(const QString& key) {
        QString title = "位置详情";
        QVector<WarriorSnapshot> filtered;
        QVector<ReplayEvent> events;

        for (const WarriorSnapshot& snapshot : report_.snapshots) {
            if (matchesLocation(snapshot, key)) filtered.push_back(snapshot);
        }
        for (const ReplayEvent& event : report_.replayEvents) {
            if (matchesLocation(event.info, key)) events.push_back(event);
        }

        if (key == "red_hq") title = "红司令部";
        else if (key == "blue_hq") title = "蓝司令部";
        else if (key.startsWith("city_")) title = "城市 " + key.mid(5);
        locationTitle_->setText(title);

        warriorTable_->setRowCount(filtered.size());
        for (int row = 0; row < filtered.size(); ++row) {
            const WarriorSnapshot& snapshot = filtered[row];
            QStringList cols = {
                snapshot.owner,
                snapshot.type,
                QString::number(snapshot.id),
                snapshot.hp >= 0 ? QString::number(snapshot.hp) : "-",
                snapshot.force >= 0 ? QString::number(snapshot.force) : "-",
                snapshot.weapon.isEmpty() ? snapshot.line : snapshot.weapon
            };
            for (int col = 0; col < cols.size(); ++col) {
                QTableWidgetItem* item = new QTableWidgetItem(cols[col]);
                if (snapshot.differs) item->setBackground(QColor("#ffebee"));
                warriorTable_->setItem(row, col, item);
            }
        }

        QString text;
        QTextStream out(&text);
        if (key == "red_hq") out << report_.redHeadquarterStatus << "\n\n";
        if (key == "blue_hq") out << report_.blueHeadquarterStatus << "\n\n";
        for (const ReplayEvent& event : events) {
            out << "[" << event.info.globalLine << "] " << event.info.text << "\n";
            if (event.differs) out << "  实际输出: " << event.actualText << "\n";
        }
        if (events.empty()) out << "该位置在回放窗口内没有筛选出的事件。";
        detailBox_->setPlainText(text);
    }

    bool matchesLocation(const WarriorSnapshot& snapshot, const QString& key) const {
        if (key == "red_hq") return snapshot.redHeadquarter;
        if (key == "blue_hq") return snapshot.blueHeadquarter;
        if (key.startsWith("city_")) return snapshot.city == key.mid(5).toInt();
        return false;
    }

    bool matchesLocation(const LineInfo& info, const QString& key) const {
        if (key == "red_hq") return info.redHeadquarter;
        if (key == "blue_hq") return info.blueHeadquarter;
        if (key.startsWith("city_")) return info.relatedCities.contains(key.mid(5).toInt());
        return false;
    }

    QComboBox* modeBox_ = nullptr;
    QLineEdit* programEdit_ = nullptr;
    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* expectedEdit_ = nullptr;
    QLineEdit* actualEdit_ = nullptr;
    QPushButton* programButton_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QSlider* citySlider_ = nullptr;
    QLabel* cityWindowLabel_ = nullptr;
    QLabel* exitCodeLabel_ = nullptr;
    QTextEdit* summaryBox_ = nullptr;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsView* view_ = nullptr;
    QTableWidget* eventTable_ = nullptr;
    QLabel* locationTitle_ = nullptr;
    QTableWidget* warriorTable_ = nullptr;
    QTextEdit* detailBox_ = nullptr;
    QVector<LocationItem*> locationItems_;
    AnalysisReport report_;
};

int main(int argc, char* argv[]) {
    QStringList args;
    for (int i = 0; i < argc; ++i) args << QString::fromLocal8Bit(argv[i]);
    if (args.size() >= 2 && args[1] == "--compare-output") {
        QString actual = args.value(2);
        QString input = args.value(3, "data.in");
        QString expected = args.value(4, "data.out");
        QTextStream out(stdout);
        if (actual.isEmpty()) {
            out << "Usage: qt_debug_tool --compare-output <actual_output> [input_file] [expected_output]\n";
            return 1;
        }
        AnalysisReport report = analyzeFiles(input, expected, actual);
        if (!report.valid) {
            out << report.error << "\n";
            return 2;
        }
        if (report.same) {
            out << "No difference found between program output and " << expected << ".\n";
            out << "Total lines checked: " << report.expectedLines.size() << "\n";
            return 0;
        }
        const LineInfo& mismatch = report.expectedInfo[report.diff.index];
        out << "First mismatch found at data.out line " << (report.diff.index + 1) << "\n";
        out << "Actual   : " << report.diff.actual << "\n";
        out << "Expected : " << report.diff.expected << "\n";
        out << "Case      : " << mismatch.caseNo << "\n";
        out << "Case line : " << mismatch.caseLine << "\n";
        out << "Time      : " << minuteToString(mismatch.minute) << "\n";
        out << "Replay window: " << minuteToString(report.replayStartMinute)
            << " -> " << minuteToString(report.mismatchMinute) << "\n";
        out << "Replay events: " << report.replayEvents.size() << "\n";
        return 1;
    }

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

#include "qt_debug_tool.moc"
