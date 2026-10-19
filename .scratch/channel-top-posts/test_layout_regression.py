"""Extract production layout methods for a native Qt geometry regression.

From the repository root in WSL:
    python3 .scratch/channel-top-posts/test_layout_regression.py . /mnt/e/TBuild/layout-regression
Then in an initialized MSVC x64 CMD (with TEMP/TMP on E:):
    cmake -S E:\\TBuild\\layout-regression -B E:\\TBuild\\layout-regression\\build -A x64 -T version=14.44
    cmake --build E:\\TBuild\\layout-regression\\build --config Debug
    E:\\TBuild\\layout-regression\\build\\Debug\\layout_regression.exe

Uses real QWidget/QScrollArea with a minimal marginless RpWidget adapter.
CMake reuses the existing static Qt/ANGLE/zlib dependencies; this does not test
Telegram session initialization, API requests, or full application painting.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1])
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)

def method(file, signature):
    text = (root / file).read_text(encoding='utf-8')
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

section = 'Telegram/SourceFiles/ayu/ui/top_posts/top_posts_section.cpp'
inner = 'Telegram/SourceFiles/ayu/ui/top_posts/top_posts_inner.cpp'
methods = '\n\n'.join([
    method(inner, 'void InnerWidget::setPosts('),
    method(inner, 'int InnerWidget::resizeGetHeight('),
    method(section, 'void SectionWidget::updateLayout('),
])
source = r'''
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QWidget>
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#include <algorithm>
#include <iostream>
#include <vector>
namespace st {
constexpr int topPostsEmptyWidth = 320;
constexpr int topPostsCardHeight = 120;
constexpr int topPostsCardMargin = 8;
constexpr int lineWidth = 1;
}
struct ChannelTopPost {};
class RpWidget : public QWidget {
public:
    using QWidget::QWidget;
    virtual int resizeGetHeight(int) = 0;
    void resizeToWidth(int width) { resize(width, resizeGetHeight(width)); }
};
class InnerWidget : public RpWidget {
public:
    using RpWidget::RpWidget;
    void setPosts(std::vector<ChannelTopPost> posts);
    int resizeGetHeight(int newWidth) override;
    std::vector<ChannelTopPost> _posts;
    int _hoveredIndex = -1;
};
class FixedBar : public RpWidget {
public:
    using RpWidget::RpWidget;
    int resizeGetHeight(int) override { return 140; }
};
class SectionWidget : public QWidget {
public:
    SectionWidget() {
        _fixedBar = new FixedBar(this);
        _fixedBarShadow = new QWidget(this);
        _scroll = new QScrollArea(this);
        _inner = new InnerWidget;
        _scroll->setWidget(_inner);
        _inner->setPosts({});
    }
    void updateLayout();
    void onScroll() {}
    FixedBar *_fixedBar;
    QWidget *_fixedBarShadow;
    QScrollArea *_scroll;
    InnerWidget *_inner;
};
'''
source += methods
source += r'''
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    int failures = 0;
    const auto check = [&](bool ok, const char *message) {
        std::cout << (ok ? "PASS " : "FAIL ") << message << std::endl;
        failures += !ok;
    };
    SectionWidget section;
    section.resize(1200, 800);
    section.show();
    app.processEvents();
    section.updateLayout();
    app.processEvents();
    check(section._inner->width() == section._scroll->width(),
        "content fills scroll width when the section opens");
    check(section._inner->height() >= st::topPostsEmptyWidth,
        "empty state has a visible paint area");
    section._inner->setPosts(std::vector<ChannelTopPost>(20));
    app.processEvents();
    check(section._inner->height() >= 20 * st::topPostsCardHeight,
        "loaded cards expand the content height");
    check(section._scroll->verticalScrollBar()->maximum() > 0,
        "cards beyond the viewport can be scrolled");
    section.resize(900, 700);
    section.updateLayout();
    app.processEvents();
    check(section._inner->width() == section._scroll->width(),
        "content follows subsequent section resize");
    section._inner->setPosts({});
    app.processEvents();
    check(section._inner->height() == st::topPostsEmptyWidth,
        "clearing cards restores empty-state height");
    return failures ? 1 : 0;
}
'''
source = source.replace('#include <QtWidgets/QWidget>', '#include <QtWidgets/QWidget>\n#include <QtWidgets/QScrollBar>')
(out / 'layout_regression.cpp').write_text(source, encoding='utf-8')
(out / 'CMakeLists.txt').write_text(
    Path(__file__).with_name('layout_regression.cmake').read_text(encoding='utf-8'),
    encoding='utf-8',
)
