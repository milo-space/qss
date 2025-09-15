from typing import Iterable, Dict, Any, List, Tuple, Optional
from PySide6.QtCore import *
from PySide6.QtGui import *
from PySide6.QtWidgets import *

# ---- 定数 / 役割ロール ----
SEARCH_ROLE       = Qt.UserRole + 100
ID_ROLE           = Qt.UserRole + 101
KATAKANA_ROLE     = Qt.UserRole + 102
HEADER_ROLE       = Qt.UserRole + 103
HISTORY_ROW_ROLE  = Qt.UserRole + 104
PLACEHOLDER_ROLE  = Qt.UserRole + 105
KATA_HIRA_OFFSET  = 0x60

_INVALID_LINEEDIT_STYLESHEET = "QLineEdit { background-color: #FFBEBE; }"
_SEPARATOR_FILL_CHAR = '─'
_SEPARATOR_FILL_LEN  = 100

HISTORY_MAX = 5
HISTORY: List[Any] = []

_PLACEHOLDER_TEXT = "未選択"

# ---- カタカナ→ローマ字変換テーブル（簡易） ----
_ROMAJI_BASE = {
    "ア":"a","イ":"i","ウ":"u","エ":"e","オ":"o",
    "カ":"ka","キ":"ki","ク":"ku","ケ":"ke","コ":"ko",
    "サ":"sa","シ":"shi","ス":"su","セ":"se","ソ":"so",
    "タ":"ta","チ":"chi","ツ":"tsu","テ":"te","ト":"to",
    "ナ":"na","ニ":"ni","ヌ":"nu","ネ":"ne","ノ":"no",
    "ハ":"ha","ヒ":"hi","フ":"fu","ヘ":"he","ホ":"ho",
    "マ":"ma","ミ":"mi","ム":"mu","メ":"me","モ":"mo",
    "ヤ":"ya","ユ":"yu","ヨ":"yo",
    "ラ":"ra","リ":"ri","ル":"ru","レ":"re","ロ":"ro",
    "ワ":"wa","ヲ":"o","ン":"n",
    "ガ":"ga","ギ":"gi","グ":"gu","ゲ":"ge","ゴ":"go",
    "ザ":"za","ジ":"ji","ズ":"zu","ゼ":"ze","ゾ":"zo",
    "ダ":"da","ヂ":"ji","ヅ":"zu","デ":"de","ド":"do",
    "バ":"ba","ビ":"bi","ブ":"bu","ベ":"be","ボ":"bo",
    "パ":"pa","ピ":"pi","プ":"pu","ペ":"pe","ポ":"po",
    "ヴ":"vu",
    "ァ":"a","ィ":"i","ゥ":"u","ェ":"e","ォ":"o",
    "ー":"-",
}
_ROMAJI_COMBOS = {
    "キャ":"kya","キュ":"kyu","キョ":"kyo",
    "シャ":"sha","シュ":"shu","ショ":"sho",
    "チャ":"cha","チュ":"chu","チョ":"cho",
    "ニャ":"nya","ニュ":"nyu","ニョ":"nyo",
    "ヒャ":"hya","ヒュ":"hyu","ヒョ":"hyo",
    "ミャ":"mya","ミュ":"myu","ミョ":"myo",
    "リャ":"rya","リュ":"ryu","リョ":"ryo",
    "ギャ":"gya","ギュ":"gyu","ギョ":"gyo",
    "ジャ":"ja","ジュ":"ju","ジョ":"jo",
    "ビャ":"bya","ビュ":"byu","ビョ":"byo",
    "ピャ":"pya","ピュ":"pyu","ピョ":"pyo",
    "ファ":"fa","フィ":"fi","フェ":"fe","フォ":"fo","フュ":"fyu",
    "ウィ":"wi","ウェ":"we","ウォ":"wo",
    "ティ":"ti","トゥ":"tu","チェ":"che",
    "シェ":"she","ジェ":"je","ディ":"di","デュ":"dyu",
    "ツァ":"tsa","ツィ":"tsi","ツェ":"tse","ツォ":"tso",
    "スィ":"si","ズィ":"zi",
    "ヴァ":"va","ヴィ":"vi","ヴェ":"ve","ヴォ":"vo","ヴュ":"vyu",
}
_SMALL = {"ャ","ュ","ョ","ァ","ィ","ゥ","ェ","ォ"}

# ---------------- IME プレエディット対応 LineEdit ----------------
class _IMEAwareLineEdit(QLineEdit):
    preeditChanged = Signal(str, str)  # (combined_text, preedit_only)

    def __init__(self, *a, **kw):
        super().__init__(*a, **kw)
        self._preedit: str = ""

    def inputMethodEvent(self, event: QInputMethodEvent):
        super().inputMethodEvent(event)
        self._preedit = event.preeditString()
        self.preeditChanged.emit(self.text() + self._preedit, self._preedit)

    def currentPreedit(self) -> str:
        return self._preedit

class _LabelReturningCompleter(QCompleter):
    def pathFromIndex(self, index):
        return index.data(Qt.DisplayRole) or ""

class _CompletionFilterModel(QSortFilterProxyModel):
    def filterAcceptsRow(self, source_row: int, source_parent: QModelIndex) -> bool:
        sm = self.sourceModel()
        idx = sm.index(source_row, 0, source_parent)
        if not idx.isValid():
            return False
        if idx.data(HEADER_ROLE):
            return False
        if idx.data(HISTORY_ROW_ROLE):
            return False
        if idx.data(PLACEHOLDER_ROLE):
            return False
        return bool(idx.data(SEARCH_ROLE))

def _katakana_to_hiragana(s: str) -> str:
    res = []
    for ch in s:
        if '\u30a1' <= ch <= '\u30f6':
            res.append(chr(ord(ch) - KATA_HIRA_OFFSET))
        else:
            res.append(ch)
    return "".join(res)

def _katakana_to_romaji(s: str) -> str:
    if not s:
        return ""
    out: List[str] = []
    i = 0
    while i < len(s):
        if s[i] == "ー":
            if out:
                last = out[-1]
                for v in ("a","i","u","e","o"):
                    if last.endswith(v):
                        out[-1] = last + v
                        break
            i += 1
            continue
        if s[i] == "ッ":
            if i + 1 < len(s):
                if i + 2 < len(s) and s[i+1:i+3] in _ROMAJI_COMBOS:
                    nxt = _ROMAJI_COMBOS[s[i+1:i+3]]
                else:
                    nxt = _ROMAJI_BASE.get(s[i+1], "")
                if nxt:
                    c = nxt[0]
                    if c not in "aeiou":
                        out.append(c)
            i += 1
            continue
        if i + 1 < len(s) and s[i+1] in _SMALL:
            combo = s[i:i+2]
            if combo in _ROMAJI_COMBOS:
                out.append(_ROMAJI_COMBOS[combo])
                i += 2
                continue
        rom = _ROMAJI_BASE.get(s[i])
        if rom:
            out.append(rom)
        i += 1
    return "".join(out)

def _build_search_blob(label: str, katakana: str, item_id: Any) -> str:
    tokens: List[str] = []
    if katakana:
        tokens.append(katakana)
        hira = _katakana_to_hiragana(katakana)
        if hira and hira != katakana:
            tokens.append(hira)
        romaji = _katakana_to_romaji(katakana)
        if romaji and romaji not in tokens:
            tokens.append(romaji)
    if label:
        tokens.append(label)
    if item_id is not None:
        tokens.append(str(item_id))
    return " ".join(dict.fromkeys(tokens))

class SearchableComboBox(QComboBox):
    validIndexChanged = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setEditable(True)
        self.setInsertPolicy(QComboBox.NoInsert)

        le = _IMEAwareLineEdit(self)
        self.setLineEdit(le)

        self.setMaxVisibleItems(20)
        self.view().setTextElideMode(Qt.ElideNone)

        self._model = QStandardItemModel(self)
        super().setModel(self._model)

        self._completion_proxy = _CompletionFilterModel(self)
        self._completion_proxy.setSourceModel(self._model)

        self._completer = _LabelReturningCompleter(self._completion_proxy, self)
        self._completer.setCompletionColumn(0)
        self._completer.setCompletionRole(SEARCH_ROLE)
        self._completer.setCaseSensitivity(Qt.CaseInsensitive)
        self._completer.setFilterMode(Qt.MatchContains)
        self._completer.activated[str].connect(self._on_completer_activated)
        # 候補移動（上下 / Tab）でハイライト変化時にも色更新
        self._completer.highlighted[str].connect(self._on_completer_highlighted)
        self.setCompleter(self._completer)

        le.returnPressed.connect(self._validate_or_revert)
        le.textEdited.connect(self._on_text_edited)
        le.editingFinished.connect(self._validate_or_revert)
        le.preeditChanged.connect(self._on_preedit_changed)
        le.setPlaceholderText(_PLACEHOLDER_TEXT)
        le.installEventFilter(self)

        self.currentIndexChanged.connect(self._on_index_changed)
        self._completer.popup().installEventFilter(self)
        self.activated.connect(self._on_any_activated)

        self._origLineEditStyle: str = le.styleSheet()

        self._lastValidIndex: int = -1
        self._base_items: List[Dict[str, Any]] = []
        self._rebuilding: bool = False
        self._id_first_row_map: Dict[Any, int] = {}
        self._placeholder_row: int = -1
        self._in_preedit: bool = False

    # ---------- QComboBox オーバーライド ----------
    def setCurrentIndex(self, index: int):
        super().setCurrentIndex(index)
        if 0 <= index == self._placeholder_row:
            self._apply_placeholder_visual()
            self._set_lineedit_invalid_state(False, force=True)

    # ---------- IME プレエディット対応 ----------
    def _on_preedit_changed(self, combined_text: str, preedit: str):
        self._in_preedit = bool(preedit)
        if self._in_preedit:
            self._completer.setCompletionPrefix(combined_text)
            self._completer.complete()
            popup = self._completer.popup()
            if popup.isVisible():
                popup.setCurrentIndex(popup.model().index(-1, -1))
            self._update_invalid_visual()
        else:
            # 確定直後に再評価
            self._update_invalid_visual()

    # ---------- 公開 API ----------
    def set_items(self, items: Iterable[Dict[str, Any]]):
        temp: List[Dict[str, Any]] = []
        temp.append({"label": "", "id": None, "katakana": "", "search_blob": ""})
        for e in items:
            item_id = e.get("id", None)
            if item_id is None:
                continue
            label = str(e.get("label", ""))
            katakana = "" if e.get("katakana") in (None, "") else str(e.get("katakana"))
            search_blob = _build_search_blob(label, katakana, item_id)
            # 表示ラベルを id 付きにしている仕様
            temp.append({
                "label": f"{item_id} : {label}",
                "id": item_id,
                "katakana": katakana,
                "search_blob": search_blob,
            })
        self._base_items = temp

        existing_ids = {x["id"] for x in self._base_items}
        removed = False
        for hid in list(HISTORY):
            if hid not in existing_ids:
                HISTORY.remove(hid)
                removed = True
        self._rebuild_model(preserve_id=None if removed else self.current_id())

    def current_id(self):
        idx = self.currentIndex()
        if idx < 0:
            return None
        item = self._model.item(idx, 0)
        return item.data(ID_ROLE) if item else None

    def setCurrentTextIfValid(self, text: str) -> bool:
        if text == "" and self._placeholder_row != -1:
            self.setCurrentIndex(self._placeholder_row)
            self._lastValidIndex = self._placeholder_row
            self._apply_placeholder_visual()
            self._set_lineedit_invalid_state(False, force=True)
            return True
        for row in range(self._model.rowCount()):
            if self._is_separator_row(row):
                continue
            itm = self._model.item(row, 0)
            if itm and itm.text() == text:
                self.setCurrentIndex(row)
                self._set_lineedit_invalid_state(False, force=True)
                return True
        return False

    def set_current_id(self, item_id: Any) -> bool:
        if item_id is None:
            if self._placeholder_row != -1:
                self.setCurrentIndex(self._placeholder_row)
            return False
        row = self._id_first_row_map.get(item_id)
        if row is not None:
            self.setCurrentIndex(row)
            return True
        if self._placeholder_row != -1:
            self.setCurrentIndex(self._placeholder_row)
        return False

    def clear_selection(self):
        if self._placeholder_row != -1:
            self.setCurrentIndex(self._placeholder_row)

    # ---------- 履歴 ----------
    def _update_history(self, item_id) -> bool:
        if item_id is None:
            return False
        if item_id in HISTORY:
            if HISTORY[0] == item_id:
                return False
            HISTORY.remove(item_id)
            HISTORY.insert(0, item_id)
            return True
        HISTORY.insert(0, item_id)
        if len(HISTORY) > HISTORY_MAX:
            del HISTORY[HISTORY_MAX:]
        return True

    # ---------- 再構築 ----------
    def _rebuild_model(self, preserve_id: Optional[Any]):
        self._rebuilding = True
        self._model.clear()
        self._id_first_row_map.clear()
        self._placeholder_row = -1

        id_map = {e["id"]: e for e in self._base_items}

        ph_entry = id_map.get(None)
        if ph_entry and ph_entry["label"] == "":
            self._append_placeholder_row()
        elif ph_entry:
            self._append_data_row(ph_entry, history_row=False)

        history_ids = [hid for hid in HISTORY if hid in id_map]
        if history_ids:
            self._append_header_row(self._build_header_text("履歴"))
            for hid in history_ids:
                self._append_data_row(id_map[hid], history_row=True)

        processed: List[Tuple[str, Dict[str, Any]]] = []
        for e in self._base_items:
            if e["id"] is None:
                continue
            processed.append((e["katakana"], e))
        processed.sort(key=lambda t: (t[0] == "", t[0]))

        current_head: Optional[str] = None
        for kata, entry in processed:
            head_char = kata[0] if kata else ""
            if head_char != current_head:
                current_head = head_char
                self._append_header_row(self._build_header_text(head_char if head_char else "#"))
            self._append_data_row(entry, history_row=False)

        if preserve_id is None:
            current_saved = self.current_id()
            if current_saved is not None:
                row = self._id_first_row_map.get(current_saved)
                if row is not None:
                    self.setCurrentIndex(row)
                else:
                    self.setCurrentIndex(self._first_selectable_row())
            else:
                self.setCurrentIndex(self._first_selectable_row())
        else:
            row = self._id_first_row_map.get(preserve_id)
            if row is not None:
                self.setCurrentIndex(row)
            else:
                self.setCurrentIndex(self._first_selectable_row())

        if self._is_placeholder_current():
            self._apply_placeholder_visual()

        self._lastValidIndex = self.currentIndex()
        self._set_lineedit_invalid_state(False, force=True)
        self._rebuilding = False
        self._completion_proxy.invalidateFilter()

    # ---------- 行生成 ----------
    def _build_header_text(self, head: str) -> str:
        return f"{_SEPARATOR_FILL_CHAR * _SEPARATOR_FILL_LEN} {head} {_SEPARATOR_FILL_CHAR}"

    def _append_placeholder_row(self):
        item = QStandardItem(_PLACEHOLDER_TEXT)
        item.setData("", SEARCH_ROLE)
        item.setData(None, ID_ROLE)
        item.setData(False, HEADER_ROLE)
        item.setData(True, PLACEHOLDER_ROLE)
        item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
        item.setForeground(QColor("#888888"))
        f = item.font()
        f.setItalic(True)
        item.setFont(f)
        row = self._model.rowCount()
        self._model.appendRow(item)
        self._placeholder_row = row
        self._id_first_row_map.setdefault(None, row)

    def _append_header_row(self, text: str):
        header_item = QStandardItem(text)
        header_item.setData("", SEARCH_ROLE)
        header_item.setData(True, HEADER_ROLE)
        header_item.setFlags(Qt.NoItemFlags)
        header_item.setForeground(QColor("#00983A"))
        header_item.setTextAlignment(Qt.AlignmentFlag.AlignRight)
        self._model.appendRow(header_item)

    def _append_data_row(self, entry: Dict[str, Any], history_row: bool):
        label = entry["label"]
        katakana = entry["katakana"]
        item_id = entry["id"]
        search_blob = "" if history_row else entry.get("search_blob", "")
        item = QStandardItem(label)
        item.setData(search_blob, SEARCH_ROLE)
        item.setData(katakana, KATAKANA_ROLE)
        item.setData(bool(history_row), HISTORY_ROW_ROLE)
        if item_id is not None:
            item.setData(item_id, ID_ROLE)
        row = self._model.rowCount()
        self._model.appendRow(item)
        if not history_row and item_id is not None and item_id not in self._id_first_row_map:
            self._id_first_row_map[item_id] = row

    # ---------- 編集 / 補完 ----------
    def _on_text_edited(self, text: str):
        if self._in_preedit:
            return
        self._completer.setCompletionPrefix(text)
        self._completer.complete()
        popup = self._completer.popup()
        if popup.isVisible():
            popup.setCurrentIndex(popup.model().index(-1, -1))
        self._update_invalid_visual()

    def _on_completer_activated(self, chosen: str):
        for row in range(self._model.rowCount()):
            if self._is_separator_row(row) or self._is_placeholder_row(row):
                continue
            itm = self._model.item(row, 0)
            if itm and itm.text() == chosen:
                self.setCurrentIndex(row)
                break
        self._set_lineedit_invalid_state(False, force=True)

    def _on_completer_highlighted(self, _label: str):
        # ハイライトが有効なら valid 扱い
        self._update_invalid_visual(from_popup=True)

    def _on_any_activated(self, idx: int):
        if idx == self._placeholder_row and idx >= 0:
            self._lastValidIndex = idx
            self._apply_placeholder_visual()
            self._set_lineedit_invalid_state(False, force=True)

    def _validate_or_revert(self):
        if self._in_preedit:
            return
        text = self.lineEdit().text()
        if text == "":
            if self._placeholder_row != -1:
                self.setCurrentIndex(self._placeholder_row)
                self._lastValidIndex = self._placeholder_row
                self._apply_placeholder_visual()
                self._set_lineedit_invalid_state(False, force=True)
                return
        match_row = -1
        for row in range(self._model.rowCount()):
            if self._is_separator_row(row) or self._is_placeholder_row(row):
                continue
            itm = self._model.item(row, 0)
            if itm and itm.text() == text:
                match_row = row
                break
        if match_row >= 0:
            if match_row != self.currentIndex():
                self.setCurrentIndex(match_row)
            self._lastValidIndex = match_row
            self._set_lineedit_invalid_state(False, force=True)
        else:
            if self._lastValidIndex >= 0:
                self.setCurrentIndex(self._lastValidIndex)
                if self._is_placeholder_current():
                    self._apply_placeholder_visual()
                else:
                    self.lineEdit().setText(self._model.item(self._lastValidIndex, 0).text())
                self._set_lineedit_invalid_state(False, force=True)
            else:
                self.lineEdit().clear()
                self._set_lineedit_invalid_state(True, force=True)

    # ---------- イベント ----------
    def eventFilter(self, obj, event):
        if event.type() == QEvent.KeyPress:
            key_event: QKeyEvent = event
            key = key_event.key()
            mods = key_event.modifiers()
            popup = self._completer.popup()
            if popup.isVisible() and (obj is self.lineEdit() or obj is popup):
                curr_valid = popup.currentIndex().isValid()
                if key == Qt.Key_Tab and not (mods & Qt.ShiftModifier):
                    if curr_valid:
                        self._move_completer_cursor(+1)
                    else:
                        self._select_popup_row(0)
                    self._update_invalid_visual(from_popup=True)
                    return True
                if key in (Qt.Key_Backtab, Qt.Key_Tab) and (mods & Qt.ShiftModifier):
                    if curr_valid:
                        self._move_completer_cursor(-1)
                    else:
                        self._select_popup_row(popup.model().rowCount() - 1)
                    self._update_invalid_visual(from_popup=True)
                    return True
                if key == Qt.Key_Down:
                    self._move_completer_cursor(+1)
                    self._update_invalid_visual(from_popup=True)
                    return True
                if key == Qt.Key_Up:
                    self._move_completer_cursor(-1)
                    self._update_invalid_visual(from_popup=True)
                    return True
                if key == Qt.Key_Escape:
                    popup.hide()
                    return True
        return super().eventFilter(obj, event)

    def _on_index_changed(self, idx: int):
        if self._rebuilding or idx < 0:
            return
        if self._is_placeholder_row(idx):
            self._lastValidIndex = idx
            self._apply_placeholder_visual()
            self.validIndexChanged.emit(idx)
            return
        if self._is_separator_row(idx):
            return
        if self._model.item(idx, 0).data(HISTORY_ROW_ROLE):
            self._lastValidIndex = idx
            self.validIndexChanged.emit(idx)
            return
        self._lastValidIndex = idx
        current_id = self._model.item(idx, 0).data(ID_ROLE)
        if self._update_history(current_id):
            self._rebuild_model(preserve_id=current_id)
        self.validIndexChanged.emit(idx)

    # ---------- ポップアップ移動 ----------
    def _select_popup_row(self, row: int):
        popup = self._completer.popup()
        model = popup.model()
        if not (0 <= row < model.rowCount()):
            return
        idx = model.index(row, 0)
        popup.setCurrentIndex(idx)
        sel = popup.selectionModel()
        if sel:
            sel.select(idx, QItemSelectionModel.ClearAndSelect | QItemSelectionModel.Rows)
        popup.scrollTo(idx, QAbstractItemView.PositionAtCenter)

    def _move_completer_cursor(self, delta: int):
        popup = self._completer.popup()
        model = popup.model()
        rows = model.rowCount()
        if rows == 0:
            return
        current = popup.currentIndex()
        if not current.isValid():
            new_row = 0 if delta > 0 else rows - 1
        else:
            new_row = (current.row() + delta) % rows
        self._select_popup_row(new_row)

    # ---------- 補助 ----------
    def _is_separator_row(self, row: int) -> bool:
        itm = self._model.item(row, 0)
        if not itm:
            return False
        if itm.data(PLACEHOLDER_ROLE):
            return False
        return bool(itm.data(HEADER_ROLE))

    def _is_placeholder_row(self, row: int) -> bool:
        itm = self._model.item(row, 0)
        return bool(itm and itm.data(PLACEHOLDER_ROLE))

    def _is_placeholder_current(self) -> bool:
        idx = self.currentIndex()
        return idx >= 0 and self._is_placeholder_row(idx)

    def _first_selectable_row(self) -> int:
        if self._placeholder_row != -1:
            return self._placeholder_row
        for r in range(self._model.rowCount()):
            if not self._is_separator_row(r):
                return r
        return -1

    def _is_exact_match(self, text: str) -> bool:
        if text == "":
            return self._placeholder_row != -1
        for r in range(self._model.rowCount()):
            if self._is_separator_row(r) or self._is_placeholder_row(r):
                continue
            itm = self._model.item(r, 0)
            if itm and itm.text() == text:
                return True
        return False

    def _apply_placeholder_visual(self):
        le = self.lineEdit()
        if le.text() != "":
            le.blockSignals(True)
            le.setText("")
            le.blockSignals(False)

    def _update_invalid_visual(self, from_popup: bool = False):
        le = self.lineEdit()
        base = le.text()
        if self._in_preedit and isinstance(le, _IMEAwareLineEdit):
            base = le.text() + le.currentPreedit()

        if self._is_exact_match(base):
            self._set_lineedit_invalid_state(False, force=True)
            return

        if from_popup:
            popup = self._completer.popup()
            if popup.isVisible() and popup.currentIndex().isValid():
                self._set_lineedit_invalid_state(False, force=True)
                return

        # それ以外は未確定/不一致
        self._set_lineedit_invalid_state(True, force=True)

    def _set_lineedit_invalid_state(self, invalid: bool, force: bool = False):
        if self._in_preedit and not force:
            return
        le = self.lineEdit()
        target = _INVALID_LINEEDIT_STYLESHEET if invalid else self._origLineEditStyle
        if le.styleSheet() != target:
            le.setStyleSheet(target)

# ================= 動作テスト =================
if __name__ == "__main__":
    import sys
    app = QApplication(sys.argv)
    w = SearchableComboBox()
    w.set_items([
        {"label": "Apple", "id": 1, "katakana": "アップル"},
        {"label": "Apple", "id": 2, "katakana": "アップル"},
        {"label": "Orange", "id": 3, "katakana": "オレンジ"},
        {"label": "Grape", "id": 4, "katakana": "グレープ"},
        {"label": "Banana", "id": 5, "katakana": "バナナ"},
        {"label": "Peach", "id": 6, "katakana": "ピーチ"},
        {"label": "Lemon", "id": 7, "katakana": "レモン"},
        {"label": "Melon", "id": 8, "katakana": "メロン"},
        {"label": "Strawberry", "id": 9, "katakana": "ストロベリー"},
        {"label": "Pineapple", "id": 10, "katakana": "パイナップル"},
    ])
    w.set_current_id(None)
    w.validIndexChanged.connect(lambda i: print("index changed:", i, "id=", w.current_id(), "HISTORY=", HISTORY))
    w.resize(240, 32)
    w.show()
    sys.exit(app.exec())
