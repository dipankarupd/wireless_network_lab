import AppKit
// usage: swift svg2png.swift in.svg out.png width height [scale]
let a = CommandLine.arguments
let w = Double(a[3])!, h = Double(a[4])!, scale = a.count > 5 ? Double(a[5])! : 1.0
guard let img = NSImage(contentsOfFile: a[1]) else { print("cannot load svg"); exit(1) }
let pw = Int(w * scale), ph = Int(h * scale)
let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pw, pixelsHigh: ph, bitsPerSample: 8,
    samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
NSColor.white.setFill(); NSRect(x: 0, y: 0, width: pw, height: ph).fill()
img.draw(in: NSRect(x: 0, y: 0, width: pw, height: ph))
NSGraphicsContext.restoreGraphicsState()
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: a[2]))
print("wrote \(pw)x\(ph)")
