#ifdef DS_PLATFORM_MACOS
#include <Cocoa/Cocoa.h>
#include "engine/dialog_window.h"

namespace diverse
{
std::pair<std::string,int> FileDialogs::openFile(const std::vector<const char*>& filter,const std::vector<const char*>& desc)
{
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:NO];
    if ( filter.size() == 0 )
    {
        //select directory
        [panel setCanChooseFiles:NO];
        [panel setCanChooseDirectories:YES];
    }
    else
    {
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
    }
    //create NSArray from filter vetcor
    NSMutableArray* nsArray = [NSMutableArray arrayWithCapacity:filter.size()];
    for (const char* f : filter)
    {
        [nsArray addObject:[NSString stringWithUTF8String:f]];
    }
    [panel setAllowedFileTypes:nsArray];
    [panel runModal];
    NSURL* url = [panel URL];
    if (url)
    {
        return {std::string([url fileSystemRepresentation]),0};
    }
    return {};   
}

std::string FileDialogs::saveFile(const std::vector<const char*>&  filter,const std::vector<const char*>& desc)
{
    NSSavePanel* panel = [NSSavePanel savePanel];
    [panel setTitle:@"Save File"];
    [panel setNameFieldLabel:@"Save As:"];

    NSMutableArray* nsArray = [NSMutableArray arrayWithCapacity:filter.size()];
    for (const char* f : filter)
    {
        [nsArray addObject:[NSString stringWithUTF8String:f]];
    }
    [panel setAllowedFileTypes:nsArray];
    [panel runModal];
    NSURL* url = [panel URL];
    if (url)
    {
        return std::string([url fileSystemRepresentation]);
    }
    return std::string();
}

void messageBox(const char* title, const char* message)
{
    NSAlert* alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithUTF8String:title]];
    [alert setInformativeText:[NSString stringWithUTF8String:message]];
    [alert runModal];
}
}
#endif
