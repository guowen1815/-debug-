#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

using namespace std;

constexpr int kReplayMinutes = 60;
constexpr int kMaxVisibleCities = 10;

// 单个测试用例的基础配置。
// 这里目前主要用 N 来判断城市边界，其他字段预留给后续扩展。
struct CaseConfig {
    int M = 0;
    int N = 0;
    int R = 0;
    int K = 0;
    int T = 0;
};

// 为标准输出中的每一行补充调试元信息。
// 这些信息后续会被 GUI 或控制台回放逻辑复用。
struct LineInfo {
    int globalLine = 0;
    int caseNo = 0;
    int caseLine = 0;
    int minute = -1;
    int city = -1;
    bool redHeadquarter = false;
    bool blueHeadquarter = false;
    vector<int> relatedCities;
    string text;
};

// 逐行比较后的结果。
struct CompareResult {
    bool same = true;
    int index = -1;
    string expected;
    string actual;
    bool expectedMissing = false;
    bool actualMissing = false;
};

// 只解析输入中的用例头部参数，不重建完整战局。
// 调试器本身依赖标准输出进行回放，因此这里读取轻量配置即可。
bool readCases(const string& path, vector<CaseConfig>& cases) {
    ifstream fin(path);
    if (!fin) return false;
    int tc = 0;
    if (!(fin >> tc)) return false;
    cases.resize(tc);
    for (int i = 0; i < tc; ++i) {
        CaseConfig cfg;
        int hp[5], atk[5];
        fin >> cfg.M >> cfg.N >> cfg.R >> cfg.K >> cfg.T;
        for (int j = 0; j < 5; ++j) fin >> hp[j];
        for (int j = 0; j < 5; ++j) fin >> atk[j];
        cases[i] = cfg;
    }
    return true;
}

// 按行读取文本文件，并兼容 Windows 下的 CRLF。
vector<string> readLines(const string& path) {
    ifstream fin(path);
    vector<string> lines;
    string line;
    while (getline(fin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

// 将形如 004:10 的时间戳解析成分钟数，便于做“前 1 小时”窗口筛选。
int parseMinute(const string& line) {
    if (line.size() < 6) return -1;
    if (!isdigit(static_cast<unsigned char>(line[0])) ||
        !isdigit(static_cast<unsigned char>(line[1])) ||
        !isdigit(static_cast<unsigned char>(line[2])) ||
        line[3] != ':' ||
        !isdigit(static_cast<unsigned char>(line[4])) ||
        !isdigit(static_cast<unsigned char>(line[5]))) {
        return -1;
    }
    int hour = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
    int minute = (line[4] - '0') * 10 + (line[5] - '0');
    return hour * 60 + minute;
}

// 将分钟数重新格式化为题目输出风格的 hhh:mm。
string minuteToString(int minute) {
    if (minute < 0) return "unknown";
    ostringstream out;
    out.fill('0');
    out.width(3);
    out << (minute / 60);
    out << ":";
    out.width(2);
    out << (minute % 60);
    return out.str();
}

// 从标准输出文本中提取涉及到的城市编号。
// 例如 “in city 3” / “to city 5” 都会被归并到 relatedCities。
vector<int> extractCities(const string& line) {
    vector<int> cities;
    static const regex cityPattern(R"(city (\d+))");
    for (sregex_iterator it(line.begin(), line.end(), cityPattern), end; it != end; ++it) {
        cities.push_back(stoi((*it)[1].str()));
    }
    sort(cities.begin(), cities.end());
    cities.erase(unique(cities.begin(), cities.end()), cities.end());
    return cities;
}

// 把标准输出的纯文本行转成带标签的结构化行。
// 这里不模拟战斗，只做“行 -> 时间/城市/司令部相关性”的映射。
vector<LineInfo> buildLineInfo(const vector<string>& lines) {
    vector<LineInfo> infos;
    int caseNo = 0;
    int caseLine = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        LineInfo info;
        info.globalLine = static_cast<int>(i) + 1;
        info.text = lines[i];
        if (lines[i].rfind("Case ", 0) == 0) {
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
        info.redHeadquarter = lines[i].find("red headquarter") != string::npos;
        info.blueHeadquarter = lines[i].find("blue headquarter") != string::npos;
        infos.push_back(info);
    }
    return infos;
}

// 逐行比较标准输出和程序实际输出，定位首个差异。
CompareResult compareLines(const vector<string>& expected, const vector<string>& actual) {
    CompareResult result;
    size_t common = min(expected.size(), actual.size());
    for (size_t i = 0; i < common; ++i) {
        if (expected[i] != actual[i]) {
            result.same = false;
            result.index = static_cast<int>(i);
            result.expected = expected[i];
            result.actual = actual[i];
            return result;
        }
    }
    if (expected.size() != actual.size()) {
        result.same = false;
        result.index = static_cast<int>(common);
        result.expectedMissing = expected.size() <= common;
        result.actualMissing = actual.size() <= common;
        if (!result.expectedMissing) result.expected = expected[common];
        if (!result.actualMissing) result.actual = actual[common];
    }
    return result;
}

// 输出差异点附近的上下文，方便在控制台快速观察前后几行。
void printExcerpt(const vector<string>& lines, int index, const string& title) {
    cout << title << "\n";
    int left = max(0, index - 2);
    int right = min(static_cast<int>(lines.size()) - 1, index + 2);
    for (int i = left; i <= right; ++i) {
        cout << (i == index ? "  > " : "    ") << (i + 1) << ": " << lines[i] << "\n";
    }
}

int clampInt(int value, int low, int high) {
    return max(low, min(value, high));
}

void addCityWindow(set<int>& focus, int n, int requestedStart) {
    if (n <= 0) return;
    int count = min(n, kMaxVisibleCities);
    int start = clampInt(requestedStart, 1, n - count + 1);
    for (int city = start; city < start + count; ++city) focus.insert(city);
}

// 为首个错误点推断“关注城市窗口”。
// 窗口最多包含 10 个城市：错误行显式提到城市时居中，司令部错误则贴近对应司令部。
set<int> buildFocusCities(const LineInfo& mismatch, const vector<CaseConfig>& cases) {
    set<int> focus;
    if (mismatch.caseNo < 1 || mismatch.caseNo > static_cast<int>(cases.size())) return focus;
    int n = cases[mismatch.caseNo - 1].N;
    if (!mismatch.relatedCities.empty()) {
        int center = mismatch.relatedCities.front();
        addCityWindow(focus, n, center - kMaxVisibleCities / 2);
        return focus;
    }
    if (mismatch.redHeadquarter) {
        addCityWindow(focus, n, 1);
        return focus;
    }
    if (mismatch.blueHeadquarter) {
        addCityWindow(focus, n, n - kMaxVisibleCities + 1);
        return focus;
    }
    addCityWindow(focus, n, 1);
    return focus;
}

// 判断某条历史事件是否与关注城市相关。
// 这是“前 1 小时事件筛选”的核心过滤条件之一。
bool touchesFocus(const LineInfo& info, const set<int>& focus) {
    if (focus.empty()) return true;
    for (int city : info.relatedCities) {
        if (focus.count(city)) return true;
    }
    if (info.redHeadquarter && (focus.count(1) || focus.count(2))) return true;
    if (info.blueHeadquarter && (focus.count(*focus.rbegin()) || focus.count(*focus.rbegin() - 1))) return true;
    return false;
}

// 输出错误发生前最近一次的两侧司令部生命元状态。
void reportHeadquarterStatus(const vector<LineInfo>& infos, int caseNo, int minute) {
    string lastRed = "No red headquarter status line found before mismatch.";
    string lastBlue = "No blue headquarter status line found before mismatch.";
    for (const LineInfo& info : infos) {
        if (info.caseNo != caseNo || info.minute < 0 || info.minute > minute) continue;
        if (info.text.find("elements in red headquarter") != string::npos) lastRed = info.text;
        if (info.text.find("elements in blue headquarter") != string::npos) lastBlue = info.text;
    }
    cout << "Headquarter status before mismatch:\n";
    cout << "  " << lastRed << "\n";
    cout << "  " << lastBlue << "\n";
}

// 生成错误点前 1 小时的控制台回放。
// 当前实现基于标准输出文本进行“事件摘录式回放”，
// 便于后续 GUI 直接复用这些筛选规则做可视化。
void reportReplayWindow(const vector<LineInfo>& infos,
                        const vector<CaseConfig>& cases,
                        const LineInfo& mismatch) {
    if (mismatch.minute < 0) {
        cout << "Cannot build replay window because the mismatch line has no timestamp.\n";
        return;
    }

    set<int> focus = buildFocusCities(mismatch, cases);
    int startMinute = max(0, mismatch.minute - kReplayMinutes);

    cout << "Replay window: " << minuteToString(startMinute) << " -> "
         << minuteToString(mismatch.minute) << "\n";
    if (!focus.empty()) {
        cout << "Focused cities:";
        for (int city : focus) cout << " " << city;
        cout << "\n";
    } else if (mismatch.redHeadquarter || mismatch.blueHeadquarter) {
        cout << "Focused area: headquarter-side events\n";
    } else {
        cout << "Focused area: fallback to all nearby logged events\n";
    }

    cout << "Relevant warrior behaviors and headquarter events:\n";
    bool found = false;
    for (const LineInfo& info : infos) {
        if (info.caseNo != mismatch.caseNo || info.minute < startMinute || info.minute > mismatch.minute) continue;
        if (!touchesFocus(info, focus) &&
            info.text.find("elements in red headquarter") == string::npos &&
            info.text.find("elements in blue headquarter") == string::npos &&
            info.text.find("reached red headquarter") == string::npos &&
            info.text.find("reached blue headquarter") == string::npos &&
            info.text.find("headquarter was taken") == string::npos) {
            continue;
        }
        found = true;
        cout << "  [line " << info.globalLine << "] " << info.text << "\n";
    }
    if (!found) cout << "  No matching replay events were found in the last 1 hour.\n";

    reportHeadquarterStatus(infos, mismatch.caseNo, mismatch.minute);
}

bool endsWith(const string& value, const string& suffix) {
    return value.size() >= suffix.size() &&
           equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

string shellQuote(const string& path) {
    string quoted = "'";
    for (char ch : path) {
        if (ch == '\'') quoted += "'\\''";
        else quoted += ch;
    }
    quoted += "'";
    return quoted;
}

int runTarget(const string& programPath,
              const string& inputPath,
              const string& outputPath) {
    string runnable = programPath;
    if (endsWith(programPath, ".cpp")) {
        string exePath = "/tmp/moshou_debug_tool_target_" + to_string(getpid());
        string compileCommand = "clang++ -std=c++17 -O2 " + shellQuote(programPath) +
                                " -o " + shellQuote(exePath);
        int compileCode = system(compileCommand.c_str());
        if (compileCode != 0) return compileCode;
        runnable = exePath;
    }
    string command = shellQuote(runnable) + " < " + shellQuote(inputPath) + " > " + shellQuote(outputPath);
    return system(command.c_str());
}

void printUsage() {
    cout << "Usage:\n";
    cout << "  debug_tool <program_or_cpp> [input_file] [expected_output] [actual_output]\n";
    cout << "  debug_tool --compare-output <actual_output> [input_file] [expected_output]\n";
    cout << "Default files: data.in data.out actual.out\n";
}

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 2) {
        printUsage();
        return 1;
    }

    bool compareOnly = string(argv[1]) == "--compare-output";
    string programPath;
    string inputPath;
    string expectedPath;
    string actualPath;

    if (compareOnly) {
        if (argc < 3) {
            printUsage();
            return 1;
        }
        actualPath = argv[2];
        inputPath = argc >= 4 ? argv[3] : "data.in";
        expectedPath = argc >= 5 ? argv[4] : "data.out";
    } else {
        programPath = argv[1];
        inputPath = argc >= 3 ? argv[2] : "data.in";
        expectedPath = argc >= 4 ? argv[3] : "data.out";
        actualPath = argc >= 5 ? argv[4] : "actual.out";
    }

    vector<CaseConfig> cases;
    if (!readCases(inputPath, cases)) {
        cerr << "Failed to read input cases from " << inputPath << "\n";
        return 2;
    }

    // 两种模式：
    // 1. 运行学生程序并抓取输出
    // 2. 直接分析已有输出文件
    if (!compareOnly) {
        int exitCode = runTarget(programPath, inputPath, actualPath);
        cout << "Program exit code: " << exitCode << "\n";
    } else {
        cout << "Compare-only mode: using existing output file " << actualPath << "\n";
    }

    vector<string> expectedLines = readLines(expectedPath);
    vector<string> actualLines = readLines(actualPath);
    vector<LineInfo> expectedInfo = buildLineInfo(expectedLines);

    CompareResult diff = compareLines(expectedLines, actualLines);
    if (diff.same) {
        cout << "No difference found between program output and " << expectedPath << ".\n";
        cout << "Total lines checked: " << expectedLines.size() << "\n";
        return 0;
    }

    int mismatchLine = diff.index + 1;
    cout << "First mismatch found at data.out line " << mismatchLine << "\n";
    if (diff.actualMissing) {
        cout << "Actual output ended early.\n";
    } else {
        cout << "Actual   : " << diff.actual << "\n";
    }
    if (diff.expectedMissing) {
        cout << "Expected output ended early.\n";
    } else {
        cout << "Expected : " << diff.expected << "\n";
    }

    // 一旦定位到首个差异，就输出最适合人工排查的几类信息：
    // 行号、Case、时间、相关城市、1 小时回放、上下文摘录。
    if (diff.index >= 0 && diff.index < static_cast<int>(expectedInfo.size())) {
        const LineInfo& mismatch = expectedInfo[diff.index];
        cout << "Case      : " << mismatch.caseNo << "\n";
        cout << "Case line : " << mismatch.caseLine << "\n";
        if (mismatch.minute >= 0) cout << "Time      : " << minuteToString(mismatch.minute) << "\n";
        if (!mismatch.relatedCities.empty()) {
            cout << "Related cities:";
            for (int city : mismatch.relatedCities) cout << " " << city;
            cout << "\n";
        } else if (mismatch.redHeadquarter) {
            cout << "Related area: red headquarter\n";
        } else if (mismatch.blueHeadquarter) {
            cout << "Related area: blue headquarter\n";
        }
        reportReplayWindow(expectedInfo, cases, mismatch);
    }

    if (diff.index < static_cast<int>(expectedLines.size())) {
        printExcerpt(expectedLines, diff.index, "Expected excerpt:");
    }
    if (diff.index < static_cast<int>(actualLines.size())) {
        printExcerpt(actualLines, diff.index, "Actual excerpt:");
    }

    return 0;
}
