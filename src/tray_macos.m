#ifdef __APPLE__

#import <Cocoa/Cocoa.h>

#include "tray.h"

static tray_callbacks callbacks;
static NSStatusItem *status_item;
static NSMenu *status_menu;
static NSMenuItem *account_item;
static NSMenuItem *state_item;
static NSMenuItem *tracked_item;
static NSMenuItem *dashboard_item;
static NSMenuItem *auth_item;
static NSMenuItem *logout_item;
static NSAutoreleasePool *autorelease_pool;

@interface KomuTrackerTrayTarget : NSObject
- (void)authAction:(id)sender;
- (void)logout:(id)sender;
- (void)openDashboard:(id)sender;
- (void)quit:(id)sender;
@end

@implementation KomuTrackerTrayTarget
- (void)authAction:(id)sender {
    (void)sender;
    if (callbacks.auth_action) callbacks.auth_action(callbacks.context);
}
- (void)logout:(id)sender {
    (void)sender;
    if (callbacks.logout) callbacks.logout(callbacks.context);
}
- (void)openDashboard:(id)sender {
    (void)sender;
    if (callbacks.open_dashboard) callbacks.open_dashboard(callbacks.context);
}
- (void)quit:(id)sender {
    (void)sender;
    if (callbacks.quit) callbacks.quit(callbacks.context);
}
@end

static KomuTrackerTrayTarget *target;

int tray_init(const tray_callbacks *provided_callbacks) {
    callbacks = *provided_callbacks;
    autorelease_pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp finishLaunching];

    target = [[KomuTrackerTrayTarget alloc] init];
    status_item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    [status_item retain];

    NSString *icon_path = [[NSBundle mainBundle] pathForResource:@"logo" ofType:@"png"];
    NSImage *icon = icon_path ? [[NSImage alloc] initWithContentsOfFile:icon_path] : nil;
    if (icon) {
        [icon setSize:NSMakeSize(18.0, 18.0)];
        status_item.button.image = icon;
        [icon release];
    } else {
        status_item.button.title = @"K";
    }
    status_item.button.toolTip = @"KomuTracker";

    status_menu = [[NSMenu alloc] initWithTitle:@"KomuTracker"];
    account_item = [[NSMenuItem alloc] initWithTitle:@"Not Logged In" action:nil keyEquivalent:@""];
    state_item = [[NSMenuItem alloc] initWithTitle:@"Starting…" action:nil keyEquivalent:@""];
    tracked_item = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [account_item setEnabled:NO];
    [state_item setEnabled:NO];
    [tracked_item setEnabled:NO];
    [status_menu addItem:account_item];
    [status_menu addItem:state_item];
    [status_menu addItem:tracked_item];
    [status_menu addItem:[NSMenuItem separatorItem]];

    dashboard_item = [[NSMenuItem alloc] initWithTitle:@"Open Dashboard"
                                                action:@selector(openDashboard:)
                                         keyEquivalent:@""];
    dashboard_item.target = target;
    [status_menu addItem:dashboard_item];

    auth_item = [[NSMenuItem alloc] initWithTitle:@"Log In"
                                           action:@selector(authAction:)
                                    keyEquivalent:@""];
    auth_item.target = target;
    [status_menu addItem:auth_item];

    logout_item = [[NSMenuItem alloc] initWithTitle:@"Log Out"
                                             action:@selector(logout:)
                                      keyEquivalent:@""];
    logout_item.target = target;
    [status_menu addItem:logout_item];
    [status_menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *quit_item = [[NSMenuItem alloc] initWithTitle:@"Quit KomuTracker"
                                                       action:@selector(quit:)
                                                keyEquivalent:@"q"];
    quit_item.target = target;
    [status_menu addItem:quit_item];
    [quit_item release];

    status_item.menu = status_menu;
    return 0;
}

void tray_update(const tray_view *view) {
    account_item.title = [NSString stringWithUTF8String:view->account_name];
    state_item.title = [NSString stringWithUTF8String:view->status_text];
    tracked_item.title = [NSString stringWithUTF8String:view->tracked_text];
    tracked_item.hidden = view->tracked_text[0] == '\0';
    dashboard_item.enabled = view->logged_in;

    BOOL authenticating = view->status == TRAY_AUTHENTICATING;
    auth_item.hidden = view->logged_in && !authenticating;
    auth_item.title = authenticating ? @"Cancel Login" : @"Log In";
    logout_item.hidden = !view->logged_in || authenticating;

    NSString *tooltip = [NSString stringWithFormat:@"KomuTracker — %@ — %@",
                          account_item.title, state_item.title];
    status_item.button.toolTip = tooltip;
}

void tray_poll(void) {
    for (;;) {
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate date]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (!event) break;
        [NSApp sendEvent:event];
    }
    [NSApp updateWindows];
    [autorelease_pool drain];
    autorelease_pool = [[NSAutoreleasePool alloc] init];
}

void tray_cleanup(void) {
    if (status_item) {
        [[NSStatusBar systemStatusBar] removeStatusItem:status_item];
        [status_item release];
        status_item = nil;
    }
    [account_item release];
    [state_item release];
    [tracked_item release];
    [dashboard_item release];
    [auth_item release];
    [logout_item release];
    [status_menu release];
    [target release];
    [autorelease_pool drain];
    autorelease_pool = nil;
}

#endif
