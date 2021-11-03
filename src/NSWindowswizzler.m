//
//  NSWindow+swizzler.m
//  Frogatto
//
//  Created by Benjamin Anderman on 11/2/21.
//  Copyright © 2021 Richard Kettering. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

@implementation NSWindow (Swizzling)

+ (void)load {
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		Class class = [self class];

		SEL originalSelector = @selector(setContentView:);
		SEL swizzledSelector = @selector(noBestResolution_setContentView:);

		Method originalMethod = class_getInstanceMethod(class, originalSelector);
		Method swizzledMethod = class_getInstanceMethod(class, swizzledSelector);

		IMP originalImp = method_getImplementation(originalMethod);
		IMP swizzledImp = method_getImplementation(swizzledMethod);

		class_replaceMethod(class,
				swizzledSelector,
				originalImp,
				method_getTypeEncoding(originalMethod));
		class_replaceMethod(class,
				originalSelector,
				swizzledImp,
				method_getTypeEncoding(swizzledMethod));

	});
}

#pragma mark - Method Swizzling

- (void)noBestResolution_setContentView:(NSView *)contentView {
	if ([contentView respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)]) {
		[contentView setWantsBestResolutionOpenGLSurface:NO];
	}
	[self noBestResolution_setContentView:contentView];
}

@end

