// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_mediator.h"

#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/badges/badge_static_item.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgeMediator () <InfobarBadgeTabHelperDelegate,
                             WebStateListObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, assign) WebStateList* webStateList;

// The consumer of the mediator.
@property(nonatomic, weak) id<BadgeConsumer> consumer;

@end

@implementation BadgeMediator
@synthesize webStateList = _webStateList;

- (instancetype)initWithConsumer:(id<BadgeConsumer>)consumer
                    webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _webStateList = webStateList;
    web::WebState* activeWebState = webStateList->GetActiveWebState();
    if (activeWebState) {
      [self updateNewWebState:activeWebState withWebStateList:webStateList];
    }
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
}

#pragma mark - InfobarBadgeTabHelperDelegate

- (void)updateInfobarBadge:(id<BadgeItem>)badgeItem {
  [self.consumer updateBadge:badgeItem];
}
- (void)addInfobarBadge:(id<BadgeItem>)badgeItem {
  [self.consumer addBadge:badgeItem];
}
- (void)removeInfobarBadge:(id<BadgeItem>)badgeItem {
  [self.consumer removeBadge:badgeItem];
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  if (newWebState && newWebState == webStateList->GetActiveWebState()) {
    [self updateNewWebState:newWebState withWebStateList:webStateList];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  // Only attempt to retrieve badges if there is a new current web state, since
  // |newWebState| can be null.
  if (newWebState) {
    [self updateNewWebState:newWebState withWebStateList:webStateList];
  }
}

#pragma mark - Private

- (void)updateNewWebState:(web::WebState*)newWebState
         withWebStateList:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);
  InfobarBadgeTabHelper* infobarBadgeTabHelper =
      InfobarBadgeTabHelper::FromWebState(newWebState);
  DCHECK(infobarBadgeTabHelper);
  infobarBadgeTabHelper->SetDelegate(self);
  // Whenever the WebState changes ask the corresponding
  // InfobarBadgeTabHelper for all the badges for that WebState.
  std::vector<id<BadgeItem>> infobar_badges =
      infobarBadgeTabHelper->GetInfobarBadgeItems();
  NSArray* infobar_badges_array =
      [NSArray arrayWithObjects:&infobar_badges[0] count:infobar_badges.size()];
  [self.consumer setupWithBadges:infobar_badges_array];
  if (newWebState->GetBrowserState()->IsOffTheRecord()) {
    BadgeStaticItem* incognitoItem = [[BadgeStaticItem alloc]
        initWithBadgeType:BadgeType::kBadgeTypeIncognito];
    [self.consumer addBadge:incognitoItem];
  }
}

@end
