// Copyright (c) 2012 House of Life Property Ltd. All rights reserved.
// Copyright (c) 2012 Crystalnix <vgachkaylo@crystalnix.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/facebook_chat/chat_item_view.h"

#include <string>

#include "base/string_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/facebook_chat/chatbar_view.h"
#include "chrome/browser/ui/views/facebook_chat/chat_notification_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/infobar_button_border.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/skia_util.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"

using views::CustomButton;
using views::Label;
using views::View;

namespace {

const gfx::Size kChatButtonSize = gfx::Size(158, 25);

const int kCloseButtonRightPadding = 3;

const int kNotificationMessageDelaySec = 10;

const int kNotifyIconDim = 16;

}

class OverOutTextButton : public views::TextButton {
public:
  OverOutTextButton(ChatItemView* owner, const std::wstring& text)
    : views::TextButton(owner, text),
      owner_(owner) {
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    owner_->OnMouseEntered(event);
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    owner_->OnMouseExited(event);
  }
private:
  ChatItemView *owner_;
};

ChatItemView::ChatItemView(FacebookChatItem *model, ChatbarView *chatbar)
  : model_(model),
    chatbar_(chatbar),
    close_button_bg_color_(0),
    chat_popup_(NULL),
    notification_popup_(NULL),
    isMouseOverNotification_(false),
    notification_icon_(NULL) {
  
  model->AddObserver(this);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  openChatButton_ = new OverOutTextButton(this, UTF8ToWide(model->username()));
  //openChatButton_->SetNormalHasBorder(true);
  openChatButton_->set_icon_placement(views::TextButton::ICON_ON_LEFT);
  openChatButton_->set_border(new InfoBarButtonBorder);
  openChatButton_->SetNormalHasBorder(true);
  openChatButton_->SetAnimationDuration(0);
  openChatButton_->SetEnabledColor(SK_ColorBLACK);
  openChatButton_->SetHighlightColor(SK_ColorBLACK);
  openChatButton_->SetHoverColor(SK_ColorBLACK);
  openChatButton_->SetFont(rb.GetFont(ResourceBundle::BaseFont));

  StatusChanged();  // sets button icon
  AddChildView(openChatButton_);

  // Add the Close Button.
  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_TAB_CLOSE));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_TAB_CLOSE_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_TAB_CLOSE_P));
  //close_button_->SetTooltipText(
  //    UTF16ToWide(l10n_util::GetStringUTF16(IDS_TOOLTIP_CLOSE_TAB)));
  //close_button_->SetAccessibleName(
  //    l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  // Disable animation so that the red danger sign shows up immediately
  // to help avoid mis-clicks.
  close_button_->SetAnimationDuration(0);
  AddChildView(close_button_);
}

ChatItemView::~ChatItemView() {
  if (model_)
    model_->RemoveObserver(this);
  if (close_button_)
    delete close_button_;
  if (openChatButton_)
    delete openChatButton_;
  if (chat_popup_) {
    chat_popup_->Close();
  }
  if (notification_icon_)
    delete notification_icon_;

  for (TimerList::iterator it = timers_.begin(); it != timers_.end(); it++) {
    if (*it && (*it)->IsRunning())
      (*it)->Stop();
    delete *it;
    *it = NULL;
  }
}

void ChatItemView::ButtonPressed(views::Button* sender, const views::Event& event) {
  if (sender == close_button_) {
    Close(true);
  } else if (sender == openChatButton_) {
    ActivateChat();
  }
}

void ChatItemView::Layout() {
  gfx::Rect bounds;
  bounds.set_x(0);
  bounds.set_y(0);
  gfx::Size sz = GetPreferredSize();
  bounds.set_size(sz);

  openChatButton_->SetBoundsRect(bounds);

  gfx::Size closeButtonSize = close_button_->GetPreferredSize();
  close_button_->SetBounds(bounds.width() - closeButtonSize.width() - kCloseButtonRightPadding,
                            bounds.height() / 2 - closeButtonSize.height() / 2,
                            closeButtonSize.width(),
                            closeButtonSize.height());

  if (notification_popup_) {
    notification_popup_->SetPositionRelativeTo(RectForNotificationPopup());
  }

  if (chat_popup_) {
    chat_popup_->SetPositionRelativeTo(RectForChatPopup());
  }
}

gfx::Size ChatItemView::GetPreferredSize() {
  return kChatButtonSize;
}

void ChatItemView::OnChatUpdated(FacebookChatItem *source) {
  DCHECK(source == model_);
  switch (source->state()) {
  case FacebookChatItem::REMOVING:
    Close(false);
    break;
  case FacebookChatItem::NUM_NOTIFICATIONS_CHANGED:
    NotifyUnread();
    break;
  case FacebookChatItem::STATUS_CHANGED:
    StatusChanged();
    break;
  }
}

void ChatItemView::AnimationProgressed(const ui::Animation* animation) {
}

void ChatItemView::StatusChanged() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (model_->num_notifications() == 0) {
    if (model_->status() == FacebookChatItem::AVAILABLE)
      openChatButton_->SetIcon(*rb.GetBitmapNamed(IDR_FACEBOOK_ONLINE_ICON_14));
    else if (model_->status() == FacebookChatItem::IDLE)
      openChatButton_->SetIcon(*rb.GetBitmapNamed(IDR_FACEBOOK_IDLE_ICON_14));
    else
      openChatButton_->SetIcon(SkBitmap::SkBitmap());
  }
}

void ChatItemView::Close(bool should_animate) {
  if (notification_popup_)
    notification_popup_->Close();
  chatbar_->Remove(this, should_animate);
}

void ChatItemView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  ResourceBundle &rb = ResourceBundle::GetSharedInstance();
  SkColor bgColor = GetThemeProvider()->GetColor(ThemeService::COLOR_TAB_TEXT);

  if (bgColor != close_button_bg_color_) {
    close_button_bg_color_ = bgColor;
    close_button_->SetBackground(close_button_bg_color_,
        rb.GetBitmapNamed(IDR_TAB_CLOSE),
        rb.GetBitmapNamed(IDR_TAB_CLOSE_MASK));
  }
}

void ChatItemView::ActivateChat() {
  if (notification_popup_)
      notification_popup_->Close();

  model_->ClearUnreadMessages();

  // open popup
  std::string urlString(chrome::kFacebookChatExtensionPrefixURL);
  urlString += chrome::kFacebookChatExtensionChatPage;
  urlString += "#";
  urlString += model_->jid();
  
  chat_popup_ = ChatPopup::Show(GURL(urlString), chatbar_->browser(), RectForChatPopup(), BubbleBorder::BOTTOM_CENTER, this);
}

const FacebookChatItem* ChatItemView::GetModel() const {
  return model_;
}

void ChatItemView::ChatPopupIsClosing(ChatPopup* popup) {
  if (popup == chat_popup_)
    chat_popup_ = NULL;
}

void ChatItemView::BubbleClosing(Bubble* bubble, bool closed_by_escape) {
  DCHECK(bubble == notification_popup_);
  notification_popup_ = NULL;
  
  for (TimerList::iterator it = timers_.begin(); it != timers_.end(); it++) {
    if (*it && (*it)->IsRunning())
      (*it)->Stop();
  }
}

void ChatItemView::NotifyUnread() {
  if (model_->num_notifications() > 0) {
    if (!notification_popup_) {
      //notification_popup_->Close();
      views::Widget* frame = BrowserView::GetBrowserViewForNativeWindow(
      chatbar_->browser()->window()->GetNativeHandle())->GetWidget();
      notification_popup_ = ChatNotificationPopup::Show(frame, RectForNotificationPopup(), BubbleBorder::BOTTOM_LEFT, this);
    }

    notification_popup_->PushMessage(model_->GetMessageAtIndex(model_->num_notifications() - 1));

    ChatTimer *timer = NULL;
    for (TimerList::iterator it = timers_.begin(); it != timers_.end(); it++) {
      if (!(*it)->IsRunning()) {
        timer = *it;
        break;
      }
    }
    if (timer == NULL) {
      timer = new ChatTimer();
      timers_.push_back(timer);
    }
    timer->Start(base::TimeDelta::FromSeconds(kNotificationMessageDelaySec), this, &ChatItemView::TimerFired);

    if (!IsVisible())
      chatbar_->PlaceFirstInOrder(this);

    UpdateNotificationIcon();
    openChatButton_->SchedulePaint();
  }
}

void ChatItemView::TimerFired() {
  if (notification_popup_)
    (void)notification_popup_->PopMessage();
}

gfx::Rect ChatItemView::RectForChatPopup() {
  View* reference_view = openChatButton_;
  gfx::Point origin;
  View::ConvertPointToScreen(reference_view, &origin);
  gfx::Rect rect = reference_view->bounds();
  rect.set_origin(origin);

  return rect;
}

gfx::Rect ChatItemView::RectForNotificationPopup() {
  View* reference_view = openChatButton_;
  gfx::Point origin;
  View::ConvertPointToScreen(reference_view, &origin);
  gfx::Rect rect = reference_view->bounds();
  rect.set_origin(origin);
  rect.set_width(20);

  return rect; 
}

void ChatItemView::OnMouseEntered(const views::MouseEvent& event) {
  if (!notification_popup_ && model_->num_notifications() > 0) {
    views::Widget* frame = BrowserView::GetBrowserViewForNativeWindow(
      chatbar_->browser()->window()->GetNativeHandle())->GetWidget();
    
    notification_popup_ = ChatNotificationPopup::Show(frame, RectForNotificationPopup(), BubbleBorder::BOTTOM_LEFT, this);
    notification_popup_->PushMessage(model_->GetMessageAtIndex(model_->num_notifications() - 1));
    isMouseOverNotification_ = true;
  }
}

void ChatItemView::OnMouseExited(const views::MouseEvent& event) {
  if (isMouseOverNotification_ && notification_popup_) {
    notification_popup_->set_fade_away_on_close(false);
    notification_popup_->Close();
  }
}

void ChatItemView::UpdateNotificationIcon() {
  if (notification_icon_) {
    delete notification_icon_;
    notification_icon_ = NULL;
  }

  if (model_->num_notifications() > 0) {
    notification_icon_ = new SkBitmap();
    notification_icon_->setConfig(SkBitmap::kARGB_8888_Config, kNotifyIconDim, kNotifyIconDim);
    notification_icon_->allocPixels();

    SkCanvas canvas(*notification_icon_);
    canvas.clear(SkColorSetARGB(0, 0, 0, 0));

    // ----------------------------------------------------------------------
    //std::string text;
    //int num = model_->num_notifications();
    //    
    //if (text.empty())
    //  return;

    //SkColor text_color = GetBadgeTextColor(tab_id);
    //SkColor background_color = GetBadgeBackgroundColor(tab_id);

    //if (SkColorGetA(text_color) == 0x00)
    //  text_color = SK_ColorWHITE;

    //if (SkColorGetA(background_color) == 0x00)
    //  background_color = SkColorSetARGB(255, 218, 0, 24);  // Default badge color.

    //canvas->Save();

    //SkPaint* text_paint = badge_util::GetBadgeTextPaintSingleton();
    //text_paint->setTextSize(SkFloatToScalar(kTextSize));
    //text_paint->setColor(text_color);

    //// Calculate text width. We clamp it to a max size.
    //SkScalar text_width = text_paint->measureText(text.c_str(), text.size());
    //text_width = SkIntToScalar(
    //    std::min(kMaxTextWidth, SkScalarFloor(text_width)));

    //// Calculate badge size. It is clamped to a min width just because it looks
    //// silly if it is too skinny.
    //int badge_width = SkScalarFloor(text_width) + kPadding * 2;
    //int icon_width = GetIcon(tab_id).width();
    //// Force the pixel width of badge to be either odd (if the icon width is odd)
    //// or even otherwise. If there is a mismatch you get http://crbug.com/26400.
    //if (icon_width != 0 && (badge_width % 2 != GetIcon(tab_id).width() % 2))
    //  badge_width += 1;
    //badge_width = std::max(kBadgeHeight, badge_width);

    //// Paint the badge background color in the right location. It is usually
    //// right-aligned, but it can also be center-aligned if it is large.
    //SkRect rect;
    //rect.fBottom = SkIntToScalar(bounds.bottom() - kBottomMargin);
    //rect.fTop = rect.fBottom - SkIntToScalar(kBadgeHeight);
    //if (badge_width >= kCenterAlignThreshold) {
    //  rect.fLeft = SkIntToScalar(
    //                   SkScalarFloor(SkIntToScalar(bounds.x()) +
    //                                 SkIntToScalar(bounds.width()) / 2 -
    //                                 SkIntToScalar(badge_width) / 2));
    //  rect.fRight = rect.fLeft + SkIntToScalar(badge_width);
    //} else {
    //  rect.fRight = SkIntToScalar(bounds.right());
    //  rect.fLeft = rect.fRight - badge_width;
    //}

    //SkPaint rect_paint;
    //rect_paint.setStyle(SkPaint::kFill_Style);
    //rect_paint.setAntiAlias(true);
    //rect_paint.setColor(background_color);
    //canvas->AsCanvasSkia()->drawRoundRect(rect, SkIntToScalar(2),
    //                                      SkIntToScalar(2), rect_paint);

    //// Overlay the gradient. It is stretchy, so we do this in three parts.
    //ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();
    //SkBitmap* gradient_left = resource_bundle.GetBitmapNamed(
    //    IDR_BROWSER_ACTION_BADGE_LEFT);
    //SkBitmap* gradient_right = resource_bundle.GetBitmapNamed(
    //    IDR_BROWSER_ACTION_BADGE_RIGHT);
    //SkBitmap* gradient_center = resource_bundle.GetBitmapNamed(
    //    IDR_BROWSER_ACTION_BADGE_CENTER);

    //canvas->AsCanvasSkia()->drawBitmap(*gradient_left, rect.fLeft, rect.fTop);
    //canvas->TileImageInt(*gradient_center,
    //    SkScalarFloor(rect.fLeft) + gradient_left->width(),
    //    SkScalarFloor(rect.fTop),
    //    SkScalarFloor(rect.width()) - gradient_left->width() -
    //                  gradient_right->width(),
    //    SkScalarFloor(rect.height()));
    //canvas->AsCanvasSkia()->drawBitmap(*gradient_right,
    //    rect.fRight - SkIntToScalar(gradient_right->width()), rect.fTop);

    //// Finally, draw the text centered within the badge. We set a clip in case the
    //// text was too large.
    //rect.fLeft += kPadding;
    //rect.fRight -= kPadding;
    //canvas->AsCanvasSkia()->clipRect(rect);
    //canvas->AsCanvasSkia()->drawText(text.c_str(), text.size(),
    //                                 rect.fLeft + (rect.width() - text_width) / 2,
    //                                 rect.fTop + kTextSize + kTopTextPadding,
    //                                 *text_paint);
    //canvas->Restore();

    // ----------------------------------------------------------------------

    SkPaint pt;
    pt.setColor(SkColorSetARGB(160, 255, 0, 0));
    pt.setAntiAlias(true);
    canvas.drawCircle(kNotifyIconDim / 2, kNotifyIconDim / 2, kNotifyIconDim / 2, pt);

    char text[4] = { '\0', '\0', '\0', '\0' };
    char *p = text;
    int num = model_->num_notifications();
    if (num > 99)
      num = 99;
    if (num > 9)
      *p++ = num / 10 + '0';
    *p = num % 10 + '0';

    pt.setColor(SK_ColorBLACK);
    pt.setTextAlign(SkPaint::kLeft_Align);

    const char kPreferredTypeface[] = "Arial";
    SkTypeface* typeface = SkTypeface::CreateFromName(
        kPreferredTypeface, SkTypeface::kNormal);
    // Skia doesn't do any font fallback---if the user is missing the font then
    // typeface will be NULL. If we don't do manual fallback then we'll crash.
    if (typeface) {
      //pt.setFakeBoldText(true);
      ; // do nothing
    } else {
      // Fall back to the system font. We don't bold it because we aren't sure
      // how it will look.
      // For the most part this code path will only be hit on Linux systems
      // that don't have Arial.
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      const gfx::Font& base_font = rb.GetFont(ResourceBundle::BaseFont);
      typeface = SkTypeface::CreateFromName(
          UTF16ToUTF8(base_font.GetFontName()).c_str(), SkTypeface::kNormal);
      DCHECK(typeface);
    }
    pt.setTypeface(typeface);
    typeface->unref();

    // make the text fit into our icon
    SkScalar textSize = 8;
    SkRect textBounds;
    while (1) {
      (void)pt.measureText(text, strlen(text), &textBounds);
      if (textBounds.width() > kNotifyIconDim || textBounds.height() > kNotifyIconDim) {
        textSize -= 1;
        pt.setTextSize(textSize);
      } else
        break;
    }

    SkScalar x = kNotifyIconDim/2.0 - textBounds.width()/2.0;
    SkScalar y = kNotifyIconDim/2.0 + textBounds.height()/2.0;
    canvas.drawText(text, strlen(text), x - 1, y - 1, pt); // SKIA expects us to give the lower-left coord of text start
 
    openChatButton_->SetIcon(*notification_icon_);
  }
}