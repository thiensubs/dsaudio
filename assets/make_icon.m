// make_icon.m - render the dsaudio app icon as a 1024x1024 PNG.
//
// One idea, drawn with CoreGraphics only: a waveform in a rounded square.
// Saturated gradient background, one bold white glyph with round caps, a top
// gloss and a rim light, so it reads at the 48 px Dock size.
//
// Build:  cc -O2 -o make_icon make_icon.m -framework CoreGraphics \
//             -framework CoreFoundation -framework ImageIO
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define S 1024.0f

int main(int argc, char **argv) {
    const char *out = (argc > 1) ? argv[1] : "AppIcon.png";

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(NULL, (size_t)S, (size_t)S, 8, 0, cs,
                                             (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    if (!ctx) { fprintf(stderr, "make_icon: no bitmap context\n"); return 1; }

    /* Rounded-rect clip, the macOS app-icon shape, inset a little so the Dock
       does not crop it. */
    CGFloat pad = S * 0.065f;
    CGFloat radius = S * 0.225f;
    CGRect box = CGRectMake(pad, pad, S - 2 * pad, S - 2 * pad);
    CGPathRef rounded = CGPathCreateWithRoundedRect(box, radius, radius, NULL);
    CGContextSaveGState(ctx);
    CGContextAddPath(ctx, rounded);
    CGContextClip(ctx);

    /* Background: a saturated blue to violet gradient. */
    CGFloat stops[] = { 0.0f, 1.0f };
    CGFloat comps[] = {
        0.298f, 0.431f, 0.984f, 1.0f,   /* #4c6ef5 */
        0.451f, 0.278f, 0.941f, 1.0f    /* #7347f0 */
    };
    CGGradientRef grad = CGGradientCreateWithColorComponents(cs, comps, stops, 2);
    CGContextDrawLinearGradient(ctx, grad, CGPointMake(0, S), CGPointMake(S, 0), 0);
    CGGradientRelease(grad);

    /* Top gloss: white at 20% over the top half. */
    CGFloat gloss[] = { 1.0f, 1.0f, 1.0f, 0.20f, 1.0f, 1.0f, 1.0f, 0.0f };
    CGGradientRef gg = CGGradientCreateWithColorComponents(cs, gloss, stops, 2);
    CGContextDrawLinearGradient(ctx, gg, CGPointMake(0, S), CGPointMake(0, S * 0.45f), 0);
    CGGradientRelease(gg);

    /* The glyph: five round-capped bars, a waveform. */
    CGPoint c = CGPointMake(S / 2.0f, S / 2.0f);
    static const float heights[] = { 0.30f, 0.52f, 0.78f, 0.52f, 0.30f };
    CGFloat lw = S * 0.075f;
    CGFloat gap = S * 0.085f;
    CGContextSetLineWidth(ctx, lw);
    CGContextSetLineCap(ctx, kCGLineCapRound);
    CGContextSetRGBStrokeColor(ctx, 1.0f, 1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 5; i++) {
        float h = heights[i] * S * 0.62f;
        CGFloat x = c.x + (i - 2) * gap;
        CGContextBeginPath(ctx);
        CGContextMoveToPoint(ctx, x, c.y - h / 2.0f);
        CGContextAddLineToPoint(ctx, x, c.y + h / 2.0f);
        CGContextStrokePath(ctx);
    }

    /* Rim light: a light stroke just inside the rounded square. */
    CGContextSetRGBStrokeColor(ctx, 1.0f, 1.0f, 1.0f, 0.28f);
    CGContextSetLineWidth(ctx, S * 0.012f);
    CGPathRef rim = CGPathCreateWithRoundedRect(
        CGRectMake(pad + S * 0.02f, pad + S * 0.02f, S - 2 * (pad + S * 0.02f),
                   S - 2 * (pad + S * 0.02f)), radius * 0.9f, radius * 0.9f, NULL);
    CGContextAddPath(ctx, rim);
    CGContextStrokePath(ctx);
    CGPathRelease(rim);

    CGContextRestoreGState(ctx);
    CGImageRef img = CGBitmapContextCreateImage(ctx);

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)out,
                                                           (CFIndex)strlen(out), false);
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL);
    if (!dest) { fprintf(stderr, "make_icon: no image destination\n"); return 1; }
    CGImageDestinationAddImage(dest, img, NULL);
    if (!CGImageDestinationFinalize(dest)) {
        fprintf(stderr, "make_icon: could not write %s\n", out);
        return 1;
    }

    CFRelease(dest);
    CFRelease(url);
    CGImageRelease(img);
    CGPathRelease(rounded);
    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);
    return 0;
}
