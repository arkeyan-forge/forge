// A real live resize of a game window on macOS, for the resize check (docs/runtime/gpu/README.md, "Window and resize").
//
// Brings the process to the front, then drags the bottom-right corner of its window with the mouse for a few seconds
// (AppKit then runs its live-resize loop, as when the player drags it), capturing only that window (screencapture -l)
// before, during and after. The cursor goes back where it was. Needs the Accessibility permission for the terminal (to post mouse events).
//
//   swiftc -O tools/perf/live_resize.swift -o <scratch>/live_resize
//   <scratch>/live_resize <pid> <out dir> [seconds] [frame times file]
//
// With the game's SKY_FRAME_TIMES file it prints the drag's start and end on that file's clock (ms since start), so
// `tools/perf/frames.py`-style gaps can be read for the drag window.
import AppKit
import CoreGraphics
import Foundation

let args = CommandLine.arguments
guard args.count >= 3, let pid = Int32(args[1]) else {
    print("usage: live_resize <pid> <out dir> [seconds] [frame times file]")
    exit(2)
}
let outDir = args[2]
let seconds = args.count > 3 ? Double(args[3]) ?? 4 : 4
let framesFile = args.count > 4 ? args[4] : nil
try? FileManager.default.createDirectory(atPath: outDir, withIntermediateDirectories: true)

func gameWindow() -> (id: CGWindowID, bounds: CGRect)? {
    let list = CGWindowListCopyWindowInfo([.optionOnScreenOnly], kCGNullWindowID) as? [[String: Any]] ?? []
    var best: (CGWindowID, CGRect)?
    for info in list {
        guard (info[kCGWindowOwnerPID as String] as? Int32) == pid, (info[kCGWindowLayer as String] as? Int) == 0,
              let number = info[kCGWindowNumber as String] as? Int,
              let dict = info[kCGWindowBounds as String] as? NSDictionary,
              let rect = CGRect(dictionaryRepresentation: dict) else { continue }
        if best == nil || rect.width * rect.height > best!.1.width * best!.1.height { best = (CGWindowID(number), rect) }
    }
    return best.map { (id: $0.0, bounds: $0.1) }
}

// The last frame's time in the SKY_FRAME_TIMES file (ms since the game's start), to put this tool's clock on it.
func lastFrameMs() -> Double? {
    guard let path = framesFile, let text = try? String(contentsOfFile: path, encoding: .utf8) else { return nil }
    return text.split(separator: "\n").last.flatMap { Double($0.split(separator: " ").first ?? "") }
}

func capture(_ id: CGWindowID, _ name: String, wait: Bool) {
    let process = Process()
    process.executableURL = URL(fileURLWithPath: "/usr/sbin/screencapture")
    process.arguments = ["-x", "-o", "-l\(id)", "\(outDir)/\(name).png"] // this window only, no shadow, no sound
    try? process.run()
    if wait { process.waitUntilExit() }
}

// The game in front first: the drag must land on its corner, not on another window over it.
NSRunningApplication(processIdentifier: pid)?.activate(options: [])
usleep(500_000)
guard let window = gameWindow() else {
    print("no on-screen window for pid \(pid)")
    exit(1)
}
print("window \(window.id) at \(window.bounds)")
let anchor = lastFrameMs()
let clock = Date()
func now() -> Double { (anchor ?? 0) + Date().timeIntervalSince(clock) * 1000 }
capture(window.id, "before", wait: true)

let saved = CGEvent(source: nil)?.location ?? .zero
let corner = CGPoint(x: window.bounds.maxX - 2, y: window.bounds.maxY - 2)
func post(_ type: CGEventType, _ at: CGPoint) {
    CGEvent(mouseEventSource: nil, mouseType: type, mouseCursorPosition: at, mouseButton: .left)?.post(tap: .cghidEventTap)
}
post(.mouseMoved, corner)
usleep(150_000)
post(.leftMouseDown, corner)
let start = now()
// Shrink and grow twice (up to 240 points), then hold the button still for the last fifth: both are live resize.
let steps = Int(seconds * 60)
var captures = 0
for step in 0...steps {
    let t = Double(step) / Double(steps)
    let phase = min(t / 0.8, 1)
    let offset = -240 * sin(phase * 2 * Double.pi * 2).magnitude
    post(.leftMouseDragged, CGPoint(x: corner.x + offset, y: corner.y + offset * 0.5625))
    if step % 30 == 15 {
        capture(window.id, String(format: "drag_%02d", captures), wait: false)
        captures += 1
    }
    usleep(16_667)
}
let end = now()
post(.leftMouseUp, corner)
usleep(100_000)
post(.mouseMoved, saved)
usleep(1_000_000)
capture(window.id, "after", wait: true)
print(String(format: "drag %.0f..%.0f ms%@ (%d captures during the drag)", start, end,
             anchor == nil ? " (this tool's clock)" : " (frame times clock)", captures))
if let after = gameWindow() { print("window after \(after.bounds)") }
