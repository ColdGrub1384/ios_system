//
//  shell.h
//  shell
//
//  Created by Nicolas Holzschuch on 25/03/2018.
//  Copyright © 2018 Nicolas Holzschuch. All rights reserved.
//

#if TARGET_OS_IPHONE || TARGET_OS_MACCATALYST
#import <UIKit/UIKit.h>
#else
#import <Foundation/Foundation.h>
#endif

//! Project version number for shell.
FOUNDATION_EXPORT double shellVersionNumber;

//! Project version string for shell.
FOUNDATION_EXPORT const unsigned char shellVersionString[];

// In this header, you should import all the public headers of your framework using statements like #import <shell/PublicHeader.h>


