// Copyright (c) 2012 House of Life Property Ltd. All rights reserved.
// Copyright (c) 2012 Crystalnix <vgachkaylo@crystalnix.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/facebook_chat/facebook_chat_item_button.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/facebook_chat/facebook_chat_item_controller.h"
#include "chrome/browser/facebook_chat/facebook_chat_item.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {
  static const CGFloat kDefaultCornerRadius = 3;
  const CGFloat kRightDecorationDim = 16;
}

@implementation FacebookChatItemCell

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
  NSRect hlBounds = NSInsetRect(cellFrame, 10, 5.5);

  if (controlView && [controlView isKindOfClass:[FacebookChatItemButton class]]) {
    FacebookChatItemButton* buttonControl = (FacebookChatItemButton*)controlView;
    FacebookChatItemController* controller = buttonControl.fbController;

    if (controller && [controller chatItem] &&
        [controller chatItem]->num_notifications() != 0) {
      NSRect hlBounds2 = NSInsetRect(hlBounds, 0.5, 2.5);
      hlBounds2.size.height -= 2;

      gfx::ScopedNSGraphicsContextSaveGState scopedGState;
      scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
      [shadow.get() setShadowOffset:NSMakeSize(1, 1)];
      [shadow setShadowBlurRadius:8];
      [shadow setShadowColor:[NSColor colorWithCalibratedRed: 0.0
                                                       green: 0.6
                                                        blue: 1.0
                                                       alpha: 1.0]];
      NSBezierPath *shadowPath =
          [NSBezierPath bezierPathWithRoundedRect:hlBounds2
                                          xRadius:kDefaultCornerRadius
                                          yRadius:kDefaultCornerRadius];
      [[NSColor whiteColor] set];

      [shadow set];

      [shadowPath fill];
    }
  }

  [super drawWithFrame:hlBounds inView:controlView];
}

- (NSRect)drawTitle:(NSAttributedString *)title withFrame:(NSRect)frame inView:(NSView *)controlView {
  frame.size.width -= kRightDecorationDim;
  return [super drawTitle:title withFrame:frame inView:controlView];
}
@end

@implementation FacebookChatItemButton

@synthesize fbController=controller_;

+ (Class)cellClass {
  return [FacebookChatItemCell class];
}

@end

