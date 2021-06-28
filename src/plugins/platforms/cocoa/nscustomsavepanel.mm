/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#import "nscustomsavepanel.h"
#import "nstextfield_keyevent.h"

@implementation NSCustomSavePanel

static NSCustomSavePanel *_instance = nil;


+(instancetype)allocWithZone:(struct _NSZone *)zone
{
    //    @synchronized (self) {
    //        // 为了防止多线程同时访问对象，造成多次分配内存空间，所以要加上线程锁
    //        if (_instance == nil) {
    //            _instance = [super allocWithZone:zone];
    //        }
    //        return _instance;
    //    }
    // 也可以使用一次性代码
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (_instance == nil) {
            _instance = [super allocWithZone:zone];
        }
    });
    return _instance;
}

// 为了严谨，也要重写copyWithZone 和 mutableCopyWithZone
-(id)copyWithZone:(NSZone *)zone
{
    return _instance;
}
-(id)mutableCopyWithZone:(NSZone *)zone
{
    return _instance;
}

//不需要计数器+1
- (id)retain {
    return self;
}

//不需要. 堆区的对象才需要
- (id)autorelease {
    return self;
}

//不需要
- (oneway void)release {
}

//不需要计数器个数. 直接返回最大无符号整数
- (NSUInteger)retainCount {
    return UINT_MAX;  //参照常量区字符串的retainCount
}

+ (NSCustomSavePanel*) customSavePanel
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _instance = [[NSCustomSavePanel alloc] init];
    });
    return _instance;
}


- (IBAction)ok:(id)sender
{
    NSString *fileName = self.nameFieldStringValue;
    fileName = [fileName stringByReplacingOccurrencesOfString:@" " withString:@""];
    if (fileName && fileName.length == 0)
    {
        return ;
    }
    
    return [super ok:sender];
}

- (NSString*) getCurFileName
{
    return self.nameFieldStringValue;
}

@end
