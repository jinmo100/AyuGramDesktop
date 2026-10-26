// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/top_posts/top_posts_column.h"

#include "ayu/ui/top_posts/top_posts_section.h"
#include "base/object_ptr.h"
#include "dialogs/dialogs_widget.h"
#include "mainwidget.h"
#include "ui/effects/animation_value.h"
#include "ui/ui_utility.h"
#include "window/window_adaptive.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QKeySequence>
#include <QShortcut>

namespace TopPosts {
namespace {

constexpr auto kSlideDuration = crl::time(200);

} // namespace

Column::Column(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Ui::RpWidget(parent)
, _controller(controller) {
	hide();
	if (const auto container = parentWidget()) {
		container->installEventFilter(this);
	}
	const auto escape = Ui::CreateChild<QShortcut>(this);
	escape->setKey(QKeySequence(Qt::Key_Escape));
	escape->setContext(Qt::WidgetWithChildrenShortcut);
	QObject::connect(escape, &QShortcut::activated, this, [=] {
		hideColumn();
	});
}

Column::~Column() = default;

void Column::showChannel(not_null<PeerData*> channel) {
	const auto container = parentWidget();
	if (!container) {
		return;
	}
	setGeometry(-container->width(), 0, container->width(), container->height());
	if (_channel != channel) {
		createWidget(channel);
	}
	_shown = true;
	show();
	raise();
	_slide.start([=] { updateSlide(); }, 0., 1., kSlideDuration);
	if (_widget) {
		_widget->setInnerFocus();
	}
}

void Column::hideColumn() {
	if (!_shown) {
		return;
	}
	_shown = false;
	_slide.setFinishedCallback([=] {
		if (!_shown) {
			hide();
		}
	});
	_slide.start([=] { updateSlide(); }, 1., 0., kSlideDuration);
	if (_closed) {
		_closed();
	}
}

void Column::setInnerFocus() {
	raise();
	if (_widget) {
		_widget->setInnerFocus();
	}
}

void Column::createWidget(not_null<PeerData*> channel) {
	_widget.destroy();
	_channel = channel;
	const auto container = parentWidget();
	const auto memento = std::make_shared<SectionMemento>(channel);
	_widget = memento->createWidget(
		this,
		_controller,
		Window::Column::First,
		QRect(QPoint(), container ? container->size() : size()));
	if (_widget) {
		static_cast<SectionWidget*>(_widget.data())->setBackHandler([=] {
			hideColumn();
		});
		_widget->show();
	}
}

void Column::updateSlide() {
	const auto container = parentWidget();
	if (!container) {
		return;
	}
	const auto width = container->width();
	const auto progress = _slide.value(_shown ? 1. : 0.);
	setGeometry(
		anim::interpolate(-width, 0, progress),
		0,
		width,
		container->height());
	if (_widget) {
		_widget->setGeometry(QRect(QPoint(), size()));
	}
	if (_shown && !_slide.animating()) {
		// WHY: the dialogs widget paints its fixed bars above the chat list,
		// so the panel has to be on top of them once it has settled.
		raise();
	}
}

bool Column::eventFilter(QObject *watched, QEvent *event) {
	if (watched == parentWidget() && event->type() == QEvent::Resize) {
		updateSlide();
	}
	return Ui::RpWidget::eventFilter(watched, event);
}

void Column::keyPressEvent(QKeyEvent *e) {
	if (e->key() == Qt::Key_Escape) {
		hideColumn();
		return;
	}
	Ui::RpWidget::keyPressEvent(e);
}

void ShowTopPosts(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> channel) {
	if (!controller->window().adaptive().isOneColumn()) {
		if (const auto dialogs = controller->content()->dialogsWidget()) {
			dialogs->showTopPosts(channel);
			return;
		}
	}
	controller->showSection(std::make_shared<SectionMemento>(channel));
}

} // namespace TopPosts
