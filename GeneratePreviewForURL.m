/*******************************************************************************
  Copyright (c) 2009, Limbic Software, Inc.
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the Limbic Software, Inc. nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY LIMBIC SOFTWARE, INC. ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL LIMBIC SOFTWARE, INC. BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>

#include <Cocoa/Cocoa.h>

/* -----------------------------------------------------------------------------
   Generate a preview for file

   This function's job is to create preview for designated file
   ----------------------------------------------------------------------------- */

OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
    //#warning To complete your generator please implement the function GeneratePreviewForURL in GeneratePreviewForURL.c
	
	NSSize canvasSize = NSMakeSize(256.0f,256.0f);
    CGContextRef cgContext = QLPreviewRequestCreateContext(preview, *(CGSize *)&canvasSize, false, NULL);
	if(cgContext) {
        NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithGraphicsPort:(void *)cgContext flipped:NO];
		
		if(context) {
			[NSGraphicsContext saveGraphicsState];
			[NSGraphicsContext setCurrentContext:context];
			[context saveGraphicsState];
			
			CGContextFillRect([context graphicsPort], CGRectMake(10,10,100,100));
			//NSFont* font1= [NSFont fontWithName:@"Helvetica" size:9.0];
			//[font1 setInContext:context];
			//[@"test" drawAtPoint:NSMakePoint(100,100) withAttributes:nil];
		
			[context restoreGraphicsState];
			[NSGraphicsContext restoreGraphicsState];
		}
	
        QLPreviewRequestFlushContext(preview, cgContext);
        CFRelease(cgContext);
    }
	
	[pool release];
	return noErr;
}

void CancelPreviewGeneration(void* thisInterface, QLPreviewRequestRef preview)
{
    // implement only if supported
}
