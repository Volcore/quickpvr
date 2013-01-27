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
#include "pvr.h"

/* -----------------------------------------------------------------------------
   Generate a preview for file

   This function's job is to create preview for designated file
   ----------------------------------------------------------------------------- */

// uncomment this to show some image info as well
//#define SHOW_INFO

extern "C" OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    // Read the PVR file
    PVRTexture pvr;

    NSString *targetCFS = [[(NSURL *)url absoluteURL] path];
    int res = pvr.load(targetCFS.UTF8String);
    if(res!=PVR_LOAD_OKAY && res!=PVR_LOAD_UNKNOWN_TYPE)
    {
        [pool release];
        return noErr;
    }
	
    // create the render context
#ifdef SHOW_INFO
	NSSize canvasSize = NSMakeSize(pvr.width+100, pvr.height);
#else
	NSSize canvasSize = NSMakeSize(pvr.width, pvr.height);
#endif
    CGContextRef cgContext = QLPreviewRequestCreateContext(preview, *(CGSize *)&canvasSize, false, NULL);
	if(cgContext) 
    {
        NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithGraphicsPort:(void *)cgContext flipped:NO];
		
		if(context) 
        {
			[NSGraphicsContext saveGraphicsState];
			[NSGraphicsContext setCurrentContext:context];
			[context saveGraphicsState];
			
			int w = pvr.width;
			int h = pvr.height;
            if(pvr.data)
            {
                uint8_t *buffer = pvr.data;
			    
			    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer, (w * h * 4), NULL);
			    
			    int bitsPerComponent = 8;
			    int bitsPerPixel = 32;
			    int bytesPerRow = 4 * w;
			    CGColorSpaceRef colorSpaceRef = CGColorSpaceCreateDeviceRGB();
			    CGBitmapInfo bitmapInfo = kCGBitmapByteOrderDefault| kCGImageAlphaLast;
			    CGColorRenderingIntent renderingIntent = kCGRenderingIntentDefault;
			    CGImageRef image = CGImageCreate(w, h, bitsPerComponent, 
			    	bitsPerPixel, bytesPerRow, colorSpaceRef, bitmapInfo, provider, 
			    	NULL, NO, renderingIntent);
          if (pvr.should_flip == true) {
            CGContextTranslateCTM(cgContext, 0.0f, h);
            CGContextScaleCTM(cgContext, 1.0f, -1.0f);
          }
			    CGContextDrawImage((CGContext*)[context graphicsPort], CGRectMake(0,0,w-1,h-1), image);
          if (pvr.should_flip == true) {
            CGContextScaleCTM(cgContext, 1.0f, -1.0f);
            CGContextTranslateCTM(cgContext, 0.0f, -h);
          }
            }

#ifdef SHOW_INFO
            CGContextSelectFont (cgContext, "Lucida Grande Bold", 10, kCGEncodingMacRoman);
            CGContextSetTextDrawingMode (cgContext, kCGTextFill);
            CGContextSetRGBFillColor (cgContext, 1.0, 1.0, 1.0, 1.0);
            CGContextSetTextPosition (cgContext, w, h-10.0);
            char str[128];
            snprintf(str, 128, "%i x %i", w, h);
            CGContextShowText (cgContext, str, strlen(str));

            CGContextSetTextPosition (cgContext, w, h-22.0);
            snprintf(str, 128, "%i bpp", pvr.bpp);
            CGContextShowText (cgContext, str, strlen(str));

            CGContextSetTextPosition (cgContext, w, h-34.0);
            snprintf(str, 128, "Format: %s", pvr.format);
            CGContextShowText (cgContext, str, strlen(str));

            CGContextSetTextPosition (cgContext, w, h-46.0);
            snprintf(str, 128, "Mipmaps: %i", pvr.numMips);
            CGContextShowText (cgContext, str, strlen(str));

            if(pvr.data==NULL)
            {
                CGContextSetTextPosition (cgContext, w, h-59.0);
                snprintf(str, 128, "(unsupported)");
                CGContextShowText (cgContext, str, strlen(str));
            }
#endif
		
			[context restoreGraphicsState];
			[NSGraphicsContext restoreGraphicsState];
		}
	
        QLPreviewRequestFlushContext(preview, cgContext);
        CFRelease(cgContext);
    }
	
	[pool release];
	return noErr;
}

extern "C" void CancelPreviewGeneration(void* thisInterface, QLPreviewRequestRef preview)
{
    // implement only if supported
}
