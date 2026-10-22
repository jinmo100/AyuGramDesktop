// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/settings_storage.h"

#include "ayu/ayu_settings.h"
#include "ayu/data/ayu_database.h"
#include "ayu/ui/boxes/edit_mark_box.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_main.h"
#include "lang_auto.h"
#include "lang/lang_text_entity.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "ui/boxes/confirm_box.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

void BuildTopPostsStorage(SectionBuilder &builder) {
	const auto controller = builder.controller();

	builder.addSubsectionTitle(tr::ayu_CategoryStorage());

	builder.addButton({
		.id = u"ayu/clearTopPostsCache"_q,
		.title = tr::ayu_ClearTopPostsCache(),
		.icon = { &st::menuIconClear },
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = tr::ayu_ClearTopPostsCacheConfirmation(tr::rich),
				.confirmed = [=](Fn<void()> &&close) {
					AyuDatabase::clearAllChannelPosts();
					controller->showToast(tr::lng_box_done(tr::now));
					close();
				},
				.confirmText = tr::ayu_ClearTopPostsCacheActionText(),
				.cancelText = tr::lng_cancel(),
				.confirmStyle = &st::attentionBoxButton,
			}));
		},
	});
	builder.addSkip();
}

void BuildTopPostsReactions(SectionBuilder &builder) {
	const auto controller = builder.controller();
	const auto settings = &AyuSettings::getInstance().topPostsSettings();
	const auto defaultPositive = QString(TopPostsSettings().positiveReactions());
	const auto defaultNegative = QString(TopPostsSettings().negativeReactions());

	builder.addSubsectionTitle(tr::ayu_TopPostsReactionsTitle());

	builder.addButton({
		.id = u"ayu/topPostsPositiveReactions"_q,
		.title = tr::ayu_TopPostsPositiveReactions(),
		.st = &st::settingsButtonNoIcon,
		.label = settings->positiveReactionsValue(),
		.onClick = [=] {
			controller->show(Box<EditMarkBox>(
				tr::ayu_TopPostsPositiveReactions(),
				settings->positiveReactions(),
				defaultPositive,
				[=](const QString &value) {
					settings->setPositiveReactions(value.simplified());
				}));
		},
	});

	builder.addButton({
		.id = u"ayu/topPostsNegativeReactions"_q,
		.title = tr::ayu_TopPostsNegativeReactions(),
		.st = &st::settingsButtonNoIcon,
		.label = settings->negativeReactionsValue(),
		.onClick = [=] {
			controller->show(Box<EditMarkBox>(
				tr::ayu_TopPostsNegativeReactions(),
				settings->negativeReactions(),
				defaultNegative,
				[=](const QString &value) {
					settings->setNegativeReactions(value.simplified());
				}));
		},
	});

	builder.addSkip();
	builder.addDividerText(tr::ayu_TopPostsReactionsAbout());
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AyuStorage::Id(),
	.parentId = AyuMain::Id(),
	.title = &tr::ayu_CategoryStorage,
	.icon = &st::menuIconArchive,
}, [](SectionBuilder &builder) {
	builder.addSkip();
	BuildTopPostsStorage(builder);
	BuildTopPostsReactions(builder);
});

} // namespace

rpl::producer<QString> AyuStorage::title() {
	return tr::ayu_CategoryStorage();
}

AyuStorage::AyuStorage(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuStorage::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

} // namespace Settings
