// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "apple_utils.h"

#import <objc/runtime.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>

namespace nx {
namespace kit {
namespace apple_utils {

std::vector<std::string> getProcessCmdLineArgs()
{
    std::vector<std::string> argumentsVector;
    NSEnumerator* e = [[[NSProcessInfo processInfo] arguments] objectEnumerator];
    NSString* argument;
    while (argument = [e nextObject])
        argumentsVector.push_back([argument UTF8String]);
    return argumentsVector;
}

const std::string getPathToExecutable()
{
    return [[[[NSProcessInfo processInfo] arguments] objectAtIndex:0] fileSystemRepresentation];
}

std::string getAppDataLocation()
{
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString* applicationSupportDirectory = [paths lastObject];
    return [applicationSupportDirectory UTF8String];
}

} // namespace apple_utils
} // namespace kit
} // namespace nx
