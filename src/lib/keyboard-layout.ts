export interface KeySpec {
  id: string;
  code: string;
  label: string;
  sub?: string;
  names: string[];
  row: number;
  x: number;
  w: number;
  h?: number;
}

const K = (
  id: string,
  code: string,
  label: string,
  names: string[],
  row: number,
  x: number,
  w = 1,
  sub?: string,
): KeySpec => ({ id, code, label, names, row, x, w, sub });

/** ANSI 104 + nav cluster. Units are key-widths. Matches OpenRGB "Key: …" names. */
export const KEYBOARD: KeySpec[] = [
  K("esc", "Escape", "Esc", ["Key: Escape", "Escape"], 0, 0, 1),
  K("f1", "F1", "F1", ["Key: F1", "F1"], 0, 2),
  K("f2", "F2", "F2", ["Key: F2", "F2"], 0, 3),
  K("f3", "F3", "F3", ["Key: F3", "F3"], 0, 4),
  K("f4", "F4", "F4", ["Key: F4", "F4"], 0, 5),
  K("f5", "F5", "F5", ["Key: F5", "F5"], 0, 6.5),
  K("f6", "F6", "F6", ["Key: F6", "F6"], 0, 7.5),
  K("f7", "F7", "F7", ["Key: F7", "F7"], 0, 8.5),
  K("f8", "F8", "F8", ["Key: F8", "F8"], 0, 9.5),
  K("f9", "F9", "F9", ["Key: F9", "F9"], 0, 11),
  K("f10", "F10", "F10", ["Key: F10", "F10"], 0, 12),
  K("f11", "F11", "F11", ["Key: F11", "F11"], 0, 13),
  K("f12", "F12", "F12", ["Key: F12", "F12"], 0, 14),
  K("prtsc", "PrintScreen", "Prt", ["Key: Print Screen", "Print Screen"], 0, 15.4, 1),
  K("scrlk", "ScrollLock", "Scr", ["Key: Scroll Lock", "Scroll Lock"], 0, 16.4, 1),
  K("pause", "Pause", "Pse", ["Key: Pause/Break", "Pause/Break", "Pause"], 0, 17.4, 1),

  K("grave", "Backquote", "`", ["Key: `", "`"], 1, 0, 1, "~"),
  K("d1", "Digit1", "1", ["Key: 1", "1"], 1, 1, 1, "!"),
  K("d2", "Digit2", "2", ["Key: 2", "2"], 1, 2, 1, "@"),
  K("d3", "Digit3", "3", ["Key: 3", "3"], 1, 3, 1, "#"),
  K("d4", "Digit4", "4", ["Key: 4", "4"], 1, 4, 1, "$"),
  K("d5", "Digit5", "5", ["Key: 5", "5"], 1, 5, 1, "%"),
  K("d6", "Digit6", "6", ["Key: 6", "6"], 1, 6, 1, "^"),
  K("d7", "Digit7", "7", ["Key: 7", "7"], 1, 7, 1, "&"),
  K("d8", "Digit8", "8", ["Key: 8", "8"], 1, 8, 1, "*"),
  K("d9", "Digit9", "9", ["Key: 9", "9"], 1, 9, 1, "("),
  K("d0", "Digit0", "0", ["Key: 0", "0"], 1, 10, 1, ")"),
  K("minus", "Minus", "-", ["Key: -", "-"], 1, 11, 1, "_"),
  K("equal", "Equal", "=", ["Key: =", "Key: +", "="], 1, 12, 1, "+"),
  K("bsp", "Backspace", "Back", ["Key: Backspace", "Backspace"], 1, 13, 2),
  K("ins", "Insert", "Ins", ["Key: Insert", "Insert"], 1, 15.4),
  K("home", "Home", "Hm", ["Key: Home", "Home"], 1, 16.4),
  K("pgup", "PageUp", "Pu", ["Key: Page Up", "Page Up"], 1, 17.4),
  K("nmlk", "NumLock", "Num", ["Key: Num Lock", "Num Lock"], 1, 18.6),
  K("npdiv", "NumpadDivide", "/", ["Key: Number Pad /", "Number Pad /"], 1, 19.6),
  K("npmul", "NumpadMultiply", "*", ["Key: Number Pad *", "Number Pad *"], 1, 20.6),
  K("npsub", "NumpadSubtract", "-", ["Key: Number Pad -", "Number Pad -"], 1, 21.6),

  K("tab", "Tab", "Tab", ["Key: Tab", "Tab"], 2, 0, 1.5),
  K("q", "KeyQ", "Q", ["Key: Q", "Q"], 2, 1.5),
  K("w", "KeyW", "W", ["Key: W", "W"], 2, 2.5),
  K("e", "KeyE", "E", ["Key: E", "E"], 2, 3.5),
  K("r", "KeyR", "R", ["Key: R", "R"], 2, 4.5),
  K("t", "KeyT", "T", ["Key: T", "T"], 2, 5.5),
  K("y", "KeyY", "Y", ["Key: Y", "Y"], 2, 6.5),
  K("u", "KeyU", "U", ["Key: U", "U"], 2, 7.5),
  K("i", "KeyI", "I", ["Key: I", "I"], 2, 8.5),
  K("o", "KeyO", "O", ["Key: O", "O"], 2, 9.5),
  K("p", "KeyP", "P", ["Key: P", "P"], 2, 10.5),
  K("lbr", "BracketLeft", "[", ["Key: [", "["], 2, 11.5, 1, "{"),
  K("rbr", "BracketRight", "]", ["Key: ]", "]"], 2, 12.5, 1, "}"),
  K("bsl", "Backslash", "\\", ["Key: \\", "Key: \\ (ANSI)", "\\"], 2, 13.5, 1.5, "|"),
  K("del", "Delete", "Del", ["Key: Delete", "Delete"], 2, 15.4),
  K("end", "End", "End", ["Key: End", "End"], 2, 16.4),
  K("pgdn", "PageDown", "Pd", ["Key: Page Down", "Page Down"], 2, 17.4),
  K("np7", "Numpad7", "7", ["Key: Number Pad 7", "Number Pad 7"], 2, 18.6),
  K("np8", "Numpad8", "8", ["Key: Number Pad 8", "Number Pad 8"], 2, 19.6),
  K("np9", "Numpad9", "9", ["Key: Number Pad 9", "Number Pad 9"], 2, 20.6),
  K("npadd", "NumpadAdd", "+", ["Key: Number Pad +", "Number Pad +"], 2, 21.6, 1),

  K("caps", "CapsLock", "Caps", ["Key: Caps Lock", "Caps Lock"], 3, 0, 1.75),
  K("a", "KeyA", "A", ["Key: A", "A"], 3, 1.75),
  K("s", "KeyS", "S", ["Key: S", "S"], 3, 2.75),
  K("d", "KeyD", "D", ["Key: D", "D"], 3, 3.75),
  K("f", "KeyF", "F", ["Key: F", "F"], 3, 4.75),
  K("g", "KeyG", "G", ["Key: G", "G"], 3, 5.75),
  K("h", "KeyH", "H", ["Key: H", "H"], 3, 6.75),
  K("j", "KeyJ", "J", ["Key: J", "J"], 3, 7.75),
  K("k", "KeyK", "K", ["Key: K", "K"], 3, 8.75),
  K("l", "KeyL", "L", ["Key: L", "L"], 3, 9.75),
  K("scl", "Semicolon", ";", ["Key: ;", ";"], 3, 10.75, 1, ":"),
  K("quot", "Quote", "'", ["Key: '", "'"], 3, 11.75, 1, '"'),
  K("ent", "Enter", "Enter", ["Key: Enter", "Enter"], 3, 12.75, 2.25),
  K("np4", "Numpad4", "4", ["Key: Number Pad 4", "Number Pad 4"], 3, 18.6),
  K("np5", "Numpad5", "5", ["Key: Number Pad 5", "Number Pad 5"], 3, 19.6),
  K("np6", "Numpad6", "6", ["Key: Number Pad 6", "Number Pad 6"], 3, 20.6),

  K("lsh", "ShiftLeft", "Shift", ["Key: Left Shift", "Left Shift"], 4, 0, 2.25),
  K("z", "KeyZ", "Z", ["Key: Z", "Z"], 4, 2.25),
  K("x", "KeyX", "X", ["Key: X", "X"], 4, 3.25),
  K("c", "KeyC", "C", ["Key: C", "C"], 4, 4.25),
  K("v", "KeyV", "V", ["Key: V", "V"], 4, 5.25),
  K("b", "KeyB", "B", ["Key: B", "B"], 4, 6.25),
  K("n", "KeyN", "N", ["Key: N", "N"], 4, 7.25),
  K("m", "KeyM", "M", ["Key: M", "M"], 4, 8.25),
  K("com", "Comma", ",", ["Key: ,", ","], 4, 9.25, 1, "<"),
  K("dot", "Period", ".", ["Key: .", "."], 4, 10.25, 1, ">"),
  K("sl", "Slash", "/", ["Key: /", "/"], 4, 11.25, 1, "?"),
  K("rsh", "ShiftRight", "Shift", ["Key: Right Shift", "Right Shift"], 4, 12.25, 2.75),
  K("up", "ArrowUp", "▲", ["Key: Up Arrow", "Up Arrow"], 4, 16.4),
  K("np1", "Numpad1", "1", ["Key: Number Pad 1", "Number Pad 1"], 4, 18.6),
  K("np2", "Numpad2", "2", ["Key: Number Pad 2", "Number Pad 2"], 4, 19.6),
  K("np3", "Numpad3", "3", ["Key: Number Pad 3", "Number Pad 3"], 4, 20.6),
  K("npent", "NumpadEnter", "Ent", ["Key: Number Pad Enter", "Number Pad Enter"], 4, 21.6, 1),

  K("lct", "ControlLeft", "Ctrl", ["Key: Left Control", "Left Control"], 5, 0, 1.25),
  K("lwn", "MetaLeft", "Win", ["Key: Left Windows", "Left Windows"], 5, 1.25, 1.25),
  K("lal", "AltLeft", "Alt", ["Key: Left Alt", "Left Alt"], 5, 2.5, 1.25),
  K("spc", "Space", "", ["Key: Space", "Space"], 5, 3.75, 6.25),
  K("ral", "AltRight", "Alt", ["Key: Right Alt", "Right Alt"], 5, 10, 1.25),
  K("rwn", "MetaRight", "Win", ["Key: Right Windows", "Right Windows"], 5, 11.25, 1.25),
  K("menu", "ContextMenu", "☰", ["Key: Menu", "Menu"], 5, 12.5, 1.25),
  K("rct", "ControlRight", "Ctrl", ["Key: Right Control", "Right Control"], 5, 13.75, 1.25),
  K("left", "ArrowLeft", "◀", ["Key: Left Arrow", "Left Arrow"], 5, 15.4),
  K("down", "ArrowDown", "▼", ["Key: Down Arrow", "Down Arrow"], 5, 16.4),
  K("right", "ArrowRight", "▶", ["Key: Right Arrow", "Right Arrow"], 5, 17.4),
  K("np0", "Numpad0", "0", ["Key: Number Pad 0", "Number Pad 0"], 5, 18.6, 2),
  K("npdot", "NumpadDecimal", ".", ["Key: Number Pad .", "Number Pad ."], 5, 20.6),
];

export const LAYOUT_WIDTH = 22.6;
export const LAYOUT_HEIGHT = 6;
export const ROW_GAP = 0.16;
export const KEY_INSET = 0.06;

export function keyCenter(key: KeySpec): { x: number; y: number } {
  return { x: key.x + key.w / 2, y: key.row + 0.5 };
}

export const KEY_BY_CODE = new Map(KEYBOARD.map((k) => [k.code, k]));
export const KEY_BY_ID = new Map(KEYBOARD.map((k) => [k.id, k]));
