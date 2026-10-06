.pragma library
// Edit canvas cursors: pure functions, kept apart so they can be tested offscreen.

// Selection handles are drawn in screen pixels, so this holds at any zoom.
var MIN_HANDLE_PX = 12
function handleSize(themeSize) { return Math.max(MIN_HANDLE_PX, themeSize) }

// The resize axis a handle shows, turned with the object and snapped to the
// nearest of the four resize cursors: 0 = ↔, 1 = ⤡ (top-left/bottom-right),
// 2 = ↕, 3 = ⤢ (top-right/bottom-left). hx, hy are 0, 0.5 or 1; rotation is
// in degrees, clockwise, as Item.rotation.
function handleAxis(hx, hy, rotation) {
    const dx = hx - 0.5, dy = hy - 0.5
    const base = dy === 0 ? 0 : dx === 0 ? 90 : dx * dy > 0 ? 45 : 135
    const a = ((base + (rotation || 0)) % 180 + 180) % 180
    return Math.round(a / 45) % 4
}
