#pragma once

#include <ernativeui/ERNativeUI.hpp>

namespace showcase {

struct Text {
    const wchar_t* name;
    const wchar_t* enabled;
    const wchar_t* intensity;
    const wchar_t* choices;
    const wchar_t* inline_preset;
    const wchar_t* popup_preset;
    const wchar_t* binary_mode;
    const wchar_t* extended_preset;
    const wchar_t* toggle_code;
    const wchar_t* large;
    const wchar_t* native_dialog;
    const wchar_t* action;
    const wchar_t* popups;
    const wchar_t* disabled;
    const wchar_t* enabled_value;
    const wchar_t* minimal;
    const wchar_t* balanced;
    const wchar_t* detailed;
    const wchar_t* maximum;
    const wchar_t* preset;
    const wchar_t* bottom;
    const wchar_t* center;
    const wchar_t* dismiss_only;
    const wchar_t* row_help;
    const wchar_t* choice_help;
    const wchar_t* large_help;
    const wchar_t* native_message;
    const wchar_t* popup_help;
    const wchar_t* popup_message;
};

struct TextInputText {
    const wchar_t* showcase;
    const wchar_t* note;
    const wchar_t* note_placeholder;
    const wchar_t* extended_note;
    const wchar_t* extended_placeholder;
};

struct ColorPickerText {
    const wchar_t* showcase;
    const wchar_t* root_accent;
    const wchar_t* subpage_accent;
};

struct SliderText {
    const wchar_t* showcase;
    const wchar_t* numeric_multiplier;
    const wchar_t* numeric_signed;
    const wchar_t* numeric_seconds;
    const wchar_t* custom_mapping;
    const wchar_t* showcase_help;
    const wchar_t* multiplier_help;
    const wchar_t* signed_help;
    const wchar_t* seconds_help;
    const wchar_t* custom_help;
};

inline bool is_vietnamese(const erui::LanguageInfo& language) noexcept {
    return language.identifier == "vietnamese";
}

inline const Text& text(const erui::LanguageInfo& language) noexcept {
    static constexpr Text english{
        L"Tarnished UI Showcase", L"Showcase Enabled", L"Showcase Intensity",
        L"Choice Rows Showcase", L"Inline Rendering Preset",
        L"Popup Rendering Preset", L"Popup Binary Mode",
        L"Popup Extended Preset", L"Toggle From Client Code",
        L"Large Settings Showcase", L"Show Native OK Dialog",
        L"Showcase Action", L"Popup Variants Showcase", L"Disabled", L"Enabled",
        L"Minimal", L"Balanced", L"Detailed", L"Maximum", L"Preset",
        L"Bottom", L"Center", L"Dismiss Only",
        L"An ERNativeUI setting provided by the showcase mod.",
        L"Compare inline values with native popup selection lists.",
        L"A large logical page demonstrating automatic pagination.",
        L"This message was queued by Tarnished UI Showcase through ERNativeUI.",
        L"Demonstrates this native dialog layout and placement.",
        L"This is a native dialog presented through ERNativeUI."};
    static constexpr Text german{
        L"Befleckten-UI-Demo", L"Demo aktiviert", L"Demo-Intensität",
        L"Auswahlzeilen-Demo", L"Direktes Darstellungsprofil",
        L"Popup-Darstellungsprofil", L"Binärer Popup-Modus",
        L"Erweitertes Popup-Profil", L"Per Mod-Code umschalten",
        L"Große Einstellungsdemo", L"Nativen OK-Dialog anzeigen",
        L"Demo-Aktion", L"Popup-Varianten", L"Deaktiviert", L"Aktiviert",
        L"Minimal", L"Ausgewogen", L"Detailliert", L"Maximum", L"Profil",
        L"Unten", L"Mitte", L"Nur schließen",
        L"Eine ERNativeUI-Einstellung des Demo-Mods.",
        L"Vergleicht direkte Werte mit nativen Popup-Auswahllisten.",
        L"Eine große logische Seite zur Demonstration der automatischen Seitennavigation.",
        L"Diese Nachricht wurde von der Befleckten-UI-Demo über ERNativeUI eingereiht.",
        L"Demonstriert dieses native Dialoglayout und seine Position.",
        L"Dies ist ein nativer Dialog, der über ERNativeUI angezeigt wird."};
    static constexpr Text french{
        L"Vitrine de l'interface Sans-éclat", L"Vitrine activée", L"Intensité de la vitrine",
        L"Vitrine des choix", L"Préréglage intégré", L"Préréglage contextuel",
        L"Mode binaire contextuel", L"Préréglage contextuel étendu",
        L"Basculer depuis le code du mod", L"Grande vitrine des réglages",
        L"Afficher la boîte de dialogue OK", L"Action de la vitrine",
        L"Variantes de boîtes de dialogue", L"Désactivé", L"Activé",
        L"Minimal", L"Équilibré", L"Détaillé", L"Maximum", L"Préréglage",
        L"Bas", L"Centre", L"Fermer uniquement",
        L"Un réglage ERNativeUI fourni par le mod de démonstration.",
        L"Compare les valeurs intégrées aux listes de sélection natives.",
        L"Une grande page logique illustrant la pagination automatique.",
        L"Ce message a été mis en attente par la vitrine via ERNativeUI.",
        L"Présente cette disposition et cette position de boîte de dialogue native.",
        L"Ceci est une boîte de dialogue native affichée par ERNativeUI."};
    static constexpr Text italian{
        L"Vetrina interfaccia Senzaluce", L"Vetrina attiva", L"Intensità vetrina",
        L"Vetrina righe di scelta", L"Profilo integrato", L"Profilo popup",
        L"Modalità binaria popup", L"Profilo popup esteso",
        L"Cambia dal codice del mod", L"Vetrina impostazioni estesa",
        L"Mostra finestra OK nativa", L"Azione vetrina", L"Varianti popup",
        L"Disattivato", L"Attivato", L"Minimo", L"Bilanciato", L"Dettagliato",
        L"Massimo", L"Profilo", L"In basso", L"Al centro", L"Solo chiusura",
        L"Un'impostazione ERNativeUI fornita dal mod dimostrativo.",
        L"Confronta i valori integrati con gli elenchi popup nativi.",
        L"Una grande pagina logica che dimostra la paginazione automatica.",
        L"Questo messaggio è stato accodato dalla vetrina tramite ERNativeUI.",
        L"Dimostra questa disposizione e posizione della finestra nativa.",
        L"Questa è una finestra nativa mostrata tramite ERNativeUI."};
    static constexpr Text korean{
        L"빛바랜 자 UI 쇼케이스", L"쇼케이스 활성화", L"쇼케이스 강도",
        L"선택 행 쇼케이스", L"인라인 렌더링 프리셋", L"팝업 렌더링 프리셋",
        L"팝업 이진 모드", L"팝업 확장 프리셋", L"모드 코드에서 전환",
        L"대형 설정 쇼케이스", L"기본 OK 대화 상자 표시", L"쇼케이스 동작",
        L"팝업 변형 쇼케이스", L"비활성화", L"활성화", L"최소", L"균형",
        L"상세", L"최대", L"프리셋", L"하단", L"중앙", L"닫기만",
        L"쇼케이스 모드가 제공하는 ERNativeUI 설정입니다.",
        L"인라인 값과 기본 팝업 선택 목록을 비교합니다.",
        L"자동 페이지 나누기를 보여 주는 큰 논리 페이지입니다.",
        L"이 메시지는 쇼케이스가 ERNativeUI를 통해 대기열에 추가했습니다.",
        L"이 기본 대화 상자 배치와 위치를 보여 줍니다.",
        L"ERNativeUI를 통해 표시된 기본 대화 상자입니다."};
    static constexpr Text spanish{
        L"Demostración de interfaz Sinluz", L"Demostración activada", L"Intensidad de demostración",
        L"Demostración de opciones", L"Preajuste integrado", L"Preajuste emergente",
        L"Modo binario emergente", L"Preajuste emergente ampliado",
        L"Alternar desde el código del mod", L"Demostración de ajustes extensa",
        L"Mostrar diálogo OK nativo", L"Acción de demostración", L"Variantes de diálogos",
        L"Desactivado", L"Activado", L"Mínimo", L"Equilibrado", L"Detallado",
        L"Máximo", L"Preajuste", L"Abajo", L"Centro", L"Solo cerrar",
        L"Un ajuste de ERNativeUI proporcionado por el mod de demostración.",
        L"Compara valores integrados con listas de selección emergentes nativas.",
        L"Una página lógica grande que demuestra la paginación automática.",
        L"Este mensaje fue puesto en cola por la demostración mediante ERNativeUI.",
        L"Demuestra esta disposición y posición de diálogo nativo.",
        L"Este es un diálogo nativo mostrado mediante ERNativeUI."};
    static constexpr Text schinese{
        L"褪色者界面展示", L"启用展示", L"展示强度", L"选项行展示",
        L"内联渲染预设", L"弹窗渲染预设", L"弹窗二元模式", L"弹窗扩展预设",
        L"通过模组代码切换", L"大型设置展示", L"显示原生“确定”对话框",
        L"展示操作", L"弹窗样式展示", L"禁用", L"启用", L"最低", L"平衡",
        L"详细", L"最高", L"预设", L"底部", L"居中", L"仅关闭",
        L"由展示模组提供的 ERNativeUI 设置。", L"比较内联值与原生弹窗选择列表。",
        L"用于展示自动分页的大型逻辑页面。",
        L"此消息由展示模组通过 ERNativeUI 加入队列。",
        L"展示此原生对话框的布局和位置。", L"这是通过 ERNativeUI 显示的原生对话框。"};
    static constexpr Text tchinese{
        L"褪色者介面展示", L"啟用展示", L"展示強度", L"選項列展示",
        L"內嵌顯示預設", L"彈出式顯示預設", L"彈出式二元模式", L"彈出式擴充預設",
        L"透過模組程式碼切換", L"大型設定展示", L"顯示原生「確定」對話框",
        L"展示動作", L"彈出式視窗樣式展示", L"停用", L"啟用", L"最低", L"平衡",
        L"詳細", L"最高", L"預設", L"底部", L"置中", L"僅關閉",
        L"由展示模組提供的 ERNativeUI 設定。", L"比較內嵌值與原生彈出式選擇清單。",
        L"用於展示自動分頁的大型邏輯頁面。",
        L"此訊息由展示模組透過 ERNativeUI 加入佇列。",
        L"展示此原生對話框的配置與位置。", L"這是透過 ERNativeUI 顯示的原生對話框。"};
    static constexpr Text russian{
        L"Демонстрация интерфейса Погасшего", L"Демонстрация включена", L"Интенсивность демонстрации",
        L"Демонстрация вариантов", L"Встроенный профиль", L"Всплывающий профиль",
        L"Двоичный режим окна", L"Расширенный профиль окна", L"Переключить из кода мода",
        L"Большая демонстрация настроек", L"Показать системное окно OK",
        L"Действие демонстрации", L"Варианты диалогов", L"Отключено", L"Включено",
        L"Минимум", L"Баланс", L"Детально", L"Максимум", L"Профиль",
        L"Снизу", L"По центру", L"Только закрытие",
        L"Настройка ERNativeUI из демонстрационного мода.",
        L"Сравнивает встроенные значения с системными списками выбора.",
        L"Большая логическая страница с автоматической разбивкой.",
        L"Это сообщение поставлено в очередь демонстрацией через ERNativeUI.",
        L"Демонстрирует вид и положение системного диалога.",
        L"Это системный диалог, показанный через ERNativeUI."};
    static constexpr Text thai{
        L"ตัวอย่าง UI ผู้มัวหมอง", L"เปิดใช้ตัวอย่าง", L"ความเข้มของตัวอย่าง",
        L"ตัวอย่างแถวตัวเลือก", L"ค่ารูปแบบในแถว", L"ค่ารูปแบบป๊อปอัป",
        L"โหมดป๊อปอัปสองค่า", L"ค่ารูปแบบป๊อปอัปเพิ่มเติม", L"สลับจากโค้ดม็อด",
        L"ตัวอย่างการตั้งค่าขนาดใหญ่", L"แสดงกล่องโต้ตอบ OK ของเกม", L"การทำงานตัวอย่าง",
        L"ตัวอย่างรูปแบบป๊อปอัป", L"ปิด", L"เปิด", L"ต่ำสุด", L"สมดุล",
        L"ละเอียด", L"สูงสุด", L"ค่ารูปแบบ", L"ด้านล่าง", L"กึ่งกลาง", L"ปิดเท่านั้น",
        L"การตั้งค่า ERNativeUI จากม็อดตัวอย่าง", L"เปรียบเทียบค่าในแถวกับรายการเลือกป๊อปอัปของเกม",
        L"หน้าตรรกะขนาดใหญ่สำหรับแสดงการแบ่งหน้าอัตโนมัติ",
        L"ข้อความนี้ถูกจัดคิวโดยม็อดตัวอย่างผ่าน ERNativeUI",
        L"แสดงรูปแบบและตำแหน่งของกล่องโต้ตอบเกม", L"นี่คือกล่องโต้ตอบเกมที่แสดงผ่าน ERNativeUI"};
    static constexpr Text japanese{
        L"褪せ人UIショーケース", L"ショーケース有効", L"ショーケース強度",
        L"選択行ショーケース", L"インライン描画プリセット", L"ポップアップ描画プリセット",
        L"ポップアップ二択モード", L"拡張ポップアッププリセット", L"Modコードから切り替え",
        L"大規模設定ショーケース", L"ネイティブOKダイアログを表示", L"ショーケース操作",
        L"ポップアップ形式ショーケース", L"無効", L"有効", L"最小", L"バランス",
        L"詳細", L"最大", L"プリセット", L"下部", L"中央", L"閉じるのみ",
        L"ショーケースModが提供するERNativeUI設定です。",
        L"インライン値とネイティブのポップアップ選択リストを比較します。",
        L"自動ページ分割を実演する大きな論理ページです。",
        L"このメッセージはショーケースからERNativeUI経由で予約されました。",
        L"このネイティブダイアログの配置と位置を実演します。",
        L"ERNativeUI経由で表示されたネイティブダイアログです。"};
    static constexpr Text polish{
        L"Prezentacja interfejsu Zmatowieńca", L"Prezentacja włączona", L"Intensywność prezentacji",
        L"Prezentacja wierszy wyboru", L"Wbudowany profil", L"Profil wyskakujący",
        L"Binarny tryb okna", L"Rozszerzony profil okna", L"Przełącz z kodu moda",
        L"Duża prezentacja ustawień", L"Pokaż natywne okno OK", L"Akcja prezentacji",
        L"Warianty okien", L"Wyłączone", L"Włączone", L"Minimum", L"Zrównoważone",
        L"Szczegółowe", L"Maksimum", L"Profil", L"Dół", L"Środek", L"Tylko zamknięcie",
        L"Ustawienie ERNativeUI udostępnione przez mod demonstracyjny.",
        L"Porównuje wartości wbudowane z natywnymi listami wyboru.",
        L"Duża strona logiczna demonstrująca automatyczną paginację.",
        L"Ta wiadomość została dodana do kolejki przez prezentację ERNativeUI.",
        L"Demonstruje ten układ i położenie natywnego okna.",
        L"To natywne okno wyświetlone przez ERNativeUI."};
    static constexpr Text arabic{
        L"عرض واجهة المشوّه", L"تفعيل العرض", L"شدة العرض", L"عرض صفوف الاختيار",
        L"إعداد عرض مضمّن", L"إعداد عرض منبثق", L"وضع منبثق ثنائي",
        L"إعداد منبثق موسع", L"التبديل من كود التعديل", L"عرض الإعدادات الكبير",
        L"إظهار مربع OK الأصلي", L"إجراء العرض", L"أنواع النوافذ المنبثقة",
        L"معطل", L"مفعل", L"أدنى", L"متوازن", L"مفصل", L"أقصى", L"إعداد مسبق",
        L"أسفل", L"الوسط", L"إغلاق فقط", L"إعداد ERNativeUI يوفره تعديل العرض.",
        L"يقارن القيم المضمنة بقوائم الاختيار المنبثقة الأصلية.",
        L"صفحة منطقية كبيرة توضح التقسيم التلقائي للصفحات.",
        L"تمت إضافة هذه الرسالة إلى الطابور بواسطة العرض عبر ERNativeUI.",
        L"يوضح تخطيط وموقع مربع الحوار الأصلي.", L"هذا مربع حوار أصلي معروض عبر ERNativeUI."};
    static constexpr Text brazilian{
        L"Demonstração da interface Maculado", L"Demonstração ativada", L"Intensidade da demonstração",
        L"Demonstração de opções", L"Predefinição integrada", L"Predefinição pop-up",
        L"Modo binário pop-up", L"Predefinição pop-up estendida",
        L"Alternar pelo código do mod", L"Demonstração de configurações extensa",
        L"Mostrar diálogo OK nativo", L"Ação de demonstração", L"Variações de diálogos",
        L"Desativado", L"Ativado", L"Mínimo", L"Equilibrado", L"Detalhado",
        L"Máximo", L"Predefinição", L"Inferior", L"Centro", L"Somente fechar",
        L"Uma configuração ERNativeUI fornecida pelo mod de demonstração.",
        L"Compara valores integrados com listas de seleção pop-up nativas.",
        L"Uma página lógica grande que demonstra a paginação automática.",
        L"Esta mensagem foi enfileirada pela demonstração através do ERNativeUI.",
        L"Demonstra este layout e posição de diálogo nativo.",
        L"Este é um diálogo nativo exibido através do ERNativeUI."};
    static constexpr Text latam{
        L"Demostración de interfaz Sinluz", L"Demostración activada", L"Intensidad de demostración",
        L"Demostración de opciones", L"Preajuste integrado", L"Preajuste emergente",
        L"Modo binario emergente", L"Preajuste emergente ampliado",
        L"Alternar desde el código del mod", L"Demostración de ajustes extensa",
        L"Mostrar diálogo OK nativo", L"Acción de demostración", L"Variantes de diálogos",
        L"Desactivado", L"Activado", L"Mínimo", L"Equilibrado", L"Detallado",
        L"Máximo", L"Preajuste", L"Abajo", L"Centro", L"Solo cerrar",
        L"Un ajuste de ERNativeUI proporcionado por el mod de demostración.",
        L"Compara valores integrados con listas de selección emergentes nativas.",
        L"Una página lógica grande que demuestra la paginación automática.",
        L"Este mensaje fue puesto en cola por la demostración mediante ERNativeUI.",
        L"Demuestra esta disposición y posición de diálogo nativo.",
        L"Este es un diálogo nativo mostrado mediante ERNativeUI."};

    static constexpr Text vietnamese{
        L"Trình diễn giao diện Tarnished", L"Bật trình diễn",
        L"Cường độ trình diễn", L"Trình diễn hàng lựa chọn",
        L"Thiết lập hiển thị nội tuyến", L"Thiết lập hiển thị bật lên",
        L"Chế độ bật lên nhị phân", L"Thiết lập bật lên mở rộng",
        L"Chuyển đổi từ mã mod", L"Trình diễn cài đặt lớn",
        L"Hiện hộp thoại OK gốc", L"Hành động trình diễn",
        L"Trình diễn các hộp thoại", L"Tắt", L"Bật",
        L"Tối thiểu", L"Cân bằng", L"Chi tiết", L"Tối đa",
        L"Thiết lập", L"Phía dưới", L"Chính giữa", L"Chỉ đóng",
        L"Một cài đặt ERNativeUI do mod trình diễn cung cấp.",
        L"So sánh giá trị nội tuyến với danh sách lựa chọn gốc.",
        L"Một trang logic lớn minh họa tính năng phân trang tự động.",
        L"Thông báo này được trình diễn xếp hàng qua ERNativeUI.",
        L"Minh họa bố cục và vị trí hộp thoại gốc này.",
        L"Đây là hộp thoại gốc được hiển thị qua ERNativeUI."};

    if (is_vietnamese(language)) return vietnamese;
    switch (language.known) {
    case erui::GameLanguage::german: return german;
    case erui::GameLanguage::french: return french;
    case erui::GameLanguage::italian: return italian;
    case erui::GameLanguage::korean: return korean;
    case erui::GameLanguage::spanish: return spanish;
    case erui::GameLanguage::chinese_simplified: return schinese;
    case erui::GameLanguage::chinese_traditional: return tchinese;
    case erui::GameLanguage::russian: return russian;
    case erui::GameLanguage::thai: return thai;
    case erui::GameLanguage::japanese: return japanese;
    case erui::GameLanguage::polish: return polish;
    case erui::GameLanguage::arabic: return arabic;
    case erui::GameLanguage::portuguese_brazil: return brazilian;
    case erui::GameLanguage::spanish_latin_america: return latam;
    default: return english;
    }
}

inline const TextInputText& text_input_text(
    const erui::LanguageInfo& language) noexcept {
    static constexpr TextInputText english{
        L"Text Input Showcase", L"Player Note", L"Enter a note",
        L"Extended Note", L"Enter longer text"};
    static constexpr TextInputText german{
        L"Texteingabe-Demo", L"Spielernotiz", L"Notiz eingeben",
        L"Erweiterte Notiz", L"Längeren Text eingeben"};
    static constexpr TextInputText french{
        L"Vitrine de saisie de texte", L"Note du joueur",
        L"Saisissez une note", L"Note étendue",
        L"Saisissez un texte plus long"};
    static constexpr TextInputText italian{
        L"Demo inserimento testo", L"Nota del giocatore",
        L"Inserisci una nota", L"Nota estesa",
        L"Inserisci un testo più lungo"};
    static constexpr TextInputText korean{
        L"텍스트 입력 쇼케이스", L"플레이어 메모", L"메모 입력",
        L"확장 메모", L"긴 텍스트 입력"};
    static constexpr TextInputText spanish{
        L"Demostración de entrada de texto", L"Nota del jugador",
        L"Escribe una nota", L"Nota ampliada",
        L"Escribe un texto más largo"};
    static constexpr TextInputText schinese{
        L"文本输入展示", L"玩家备注", L"输入备注", L"扩展备注",
        L"输入更长的文本"};
    static constexpr TextInputText tchinese{
        L"文字輸入展示", L"玩家備註", L"輸入備註", L"擴充備註",
        L"輸入更長的文字"};
    static constexpr TextInputText russian{
        L"Демонстрация ввода текста", L"Заметка игрока",
        L"Введите заметку", L"Расширенная заметка",
        L"Введите более длинный текст"};
    static constexpr TextInputText thai{
        L"ตัวอย่างการป้อนข้อความ", L"บันทึกผู้เล่น", L"ป้อนบันทึก",
        L"บันทึกแบบยาว", L"ป้อนข้อความที่ยาวขึ้น"};
    static constexpr TextInputText japanese{
        L"テキスト入力ショーケース", L"プレイヤーメモ", L"メモを入力",
        L"拡張メモ", L"長いテキストを入力"};
    static constexpr TextInputText polish{
        L"Prezentacja pola tekstowego", L"Notatka gracza",
        L"Wpisz notatkę", L"Rozszerzona notatka",
        L"Wpisz dłuższy tekst"};
    static constexpr TextInputText arabic{
        L"عرض إدخال النص", L"ملاحظة اللاعب", L"أدخل ملاحظة",
        L"ملاحظة موسعة", L"أدخل نصًا أطول"};
    static constexpr TextInputText brazilian{
        L"Demonstração de entrada de texto", L"Nota do jogador",
        L"Digite uma nota", L"Nota ampliada",
        L"Digite um texto mais longo"};
    static constexpr TextInputText latam{
        L"Demostración de entrada de texto", L"Nota del jugador",
        L"Escribe una nota", L"Nota ampliada",
        L"Escribe un texto más largo"};
    static constexpr TextInputText vietnamese{
        L"Trình diễn nhập văn bản", L"Ghi chú người chơi",
        L"Nhập ghi chú", L"Ghi chú mở rộng",
        L"Nhập văn bản dài hơn"};

    if (is_vietnamese(language)) return vietnamese;
    switch (language.known) {
    case erui::GameLanguage::german: return german;
    case erui::GameLanguage::french: return french;
    case erui::GameLanguage::italian: return italian;
    case erui::GameLanguage::korean: return korean;
    case erui::GameLanguage::spanish: return spanish;
    case erui::GameLanguage::chinese_simplified: return schinese;
    case erui::GameLanguage::chinese_traditional: return tchinese;
    case erui::GameLanguage::russian: return russian;
    case erui::GameLanguage::thai: return thai;
    case erui::GameLanguage::japanese: return japanese;
    case erui::GameLanguage::polish: return polish;
    case erui::GameLanguage::arabic: return arabic;
    case erui::GameLanguage::portuguese_brazil: return brazilian;
    case erui::GameLanguage::spanish_latin_america: return latam;
    default: return english;
    }
}

inline const ColorPickerText& color_picker_text(
    const erui::LanguageInfo& language) noexcept {
    static constexpr ColorPickerText english{
        L"Color Picker Showcase", L"Root Accent Color",
        L"Subpage Accent Color"};
    static constexpr ColorPickerText german{
        L"Farbauswahl-Demo", L"Akzentfarbe der Hauptseite",
        L"Akzentfarbe der Unterseite"};
    static constexpr ColorPickerText french{
        L"Vitrine du sélecteur de couleur", L"Couleur d'accent principale",
        L"Couleur d'accent de la sous-page"};
    static constexpr ColorPickerText italian{
        L"Demo selettore colore", L"Colore principale",
        L"Colore della sottopagina"};
    static constexpr ColorPickerText korean{
        L"색상 선택 쇼케이스", L"기본 페이지 강조색",
        L"하위 페이지 강조색"};
    static constexpr ColorPickerText spanish{
        L"Demostración del selector de color", L"Color de acento principal",
        L"Color de acento de la subpágina"};
    static constexpr ColorPickerText schinese{
        L"颜色选择器展示", L"主页面强调色", L"子页面强调色"};
    static constexpr ColorPickerText tchinese{
        L"顏色選擇器展示", L"主頁面強調色", L"子頁面強調色"};
    static constexpr ColorPickerText russian{
        L"Демонстрация выбора цвета", L"Цвет главной страницы",
        L"Цвет вложенной страницы"};
    static constexpr ColorPickerText thai{
        L"ตัวอย่างตัวเลือกสี", L"สีเน้นหน้าหลัก", L"สีเน้นหน้าย่อย"};
    static constexpr ColorPickerText japanese{
        L"カラーピッカーショーケース", L"メインページのアクセント色",
        L"サブページのアクセント色"};
    static constexpr ColorPickerText polish{
        L"Prezentacja wyboru koloru", L"Kolor strony głównej",
        L"Kolor podstrony"};
    static constexpr ColorPickerText arabic{
        L"عرض منتقي الألوان", L"لون تمييز الصفحة الرئيسية",
        L"لون تمييز الصفحة الفرعية"};
    static constexpr ColorPickerText brazilian{
        L"Demonstração do seletor de cores", L"Cor de destaque principal",
        L"Cor de destaque da subpágina"};
    static constexpr ColorPickerText latam{
        L"Demostración del selector de color", L"Color de acento principal",
        L"Color de acento de la subpágina"};
    static constexpr ColorPickerText vietnamese{
        L"Trình diễn bộ chọn màu", L"Màu nhấn trang chính",
        L"Màu nhấn trang con"};

    if (is_vietnamese(language)) return vietnamese;
    switch (language.known) {
    case erui::GameLanguage::german: return german;
    case erui::GameLanguage::french: return french;
    case erui::GameLanguage::italian: return italian;
    case erui::GameLanguage::korean: return korean;
    case erui::GameLanguage::spanish: return spanish;
    case erui::GameLanguage::chinese_simplified: return schinese;
    case erui::GameLanguage::chinese_traditional: return tchinese;
    case erui::GameLanguage::russian: return russian;
    case erui::GameLanguage::thai: return thai;
    case erui::GameLanguage::japanese: return japanese;
    case erui::GameLanguage::polish: return polish;
    case erui::GameLanguage::arabic: return arabic;
    case erui::GameLanguage::portuguese_brazil: return brazilian;
    case erui::GameLanguage::spanish_latin_america: return latam;
    default: return english;
    }
}

inline const SliderText& slider_text(
    const erui::LanguageInfo& language) noexcept {
    static constexpr SliderText english{
        L"Advanced Slider Showcase", L"Camera Distance",
        L"Signed Offset", L"Path Time", L"Impossible Values",
        L"Signed decimals, units, prefixes, and arbitrary value labels.",
        L"A numeric slider displayed with an x prefix and 0.1 steps.",
        L"A numeric slider that crosses zero and displays negative values.",
        L"A numeric slider displayed in seconds.",
        L"A custom slider whose labels do not need to be numbers."};
    static constexpr SliderText german{
        L"Demo erweiterter Schieberegler", L"Kameraabstand",
        L"Vorzeichenbehafteter Versatz", L"Pfadzeit", L"Unmögliche Werte",
        L"Vorzeichen, Dezimalstellen, Einheiten, Präfixe und freie Werttexte.",
        L"Ein numerischer Schieberegler mit x-Präfix und 0,1-Schritten.",
        L"Ein numerischer Schieberegler mit negativen Werten und Nulldurchgang.",
        L"Ein numerischer Schieberegler mit Anzeige in Sekunden.",
        L"Ein freier Schieberegler, dessen Werte keine Zahlen sein müssen."};
    static constexpr SliderText french{
        L"Vitrine des curseurs avancés", L"Distance de la caméra",
        L"Décalage signé", L"Durée du trajet", L"Valeurs impossibles",
        L"Décimales signées, unités, préfixes et libellés arbitraires.",
        L"Un curseur numérique avec le préfixe x et des pas de 0,1.",
        L"Un curseur numérique qui traverse zéro et affiche des valeurs négatives.",
        L"Un curseur numérique affiché en secondes.",
        L"Un curseur personnalisé dont les libellés ne sont pas forcément des nombres."};
    static constexpr SliderText italian{
        L"Demo cursori avanzati", L"Distanza telecamera",
        L"Scostamento con segno", L"Durata percorso", L"Valori impossibili",
        L"Decimali con segno, unità, prefissi ed etichette arbitrarie.",
        L"Un cursore numerico con prefisso x e incrementi di 0,1.",
        L"Un cursore numerico che attraversa lo zero e mostra valori negativi.",
        L"Un cursore numerico visualizzato in secondi.",
        L"Un cursore personalizzato con etichette che non devono essere numeri."};
    static constexpr SliderText korean{
        L"고급 슬라이더 쇼케이스", L"카메라 거리", L"부호 오프셋",
        L"경로 시간", L"불가능한 값",
        L"부호 소수, 단위, 접두사 및 임의의 값 표시를 보여 줍니다.",
        L"x 접두사와 0.1 단위를 사용하는 숫자 슬라이더입니다.",
        L"0을 지나 음수를 표시하는 숫자 슬라이더입니다.",
        L"초 단위로 표시되는 숫자 슬라이더입니다.",
        L"표시 값이 숫자일 필요가 없는 사용자 정의 슬라이더입니다."};
    static constexpr SliderText spanish{
        L"Demostración de controles deslizantes avanzados", L"Distancia de cámara",
        L"Desplazamiento con signo", L"Tiempo de recorrido", L"Valores imposibles",
        L"Decimales con signo, unidades, prefijos y etiquetas arbitrarias.",
        L"Un control numérico con prefijo x y pasos de 0,1.",
        L"Un control numérico que cruza cero y muestra valores negativos.",
        L"Un control numérico mostrado en segundos.",
        L"Un control personalizado cuyas etiquetas no tienen que ser números."};
    static constexpr SliderText schinese{
        L"高级滑块展示", L"镜头距离", L"有符号偏移", L"路径时间",
        L"不可能的值", L"展示有符号小数、单位、前缀和任意标签。",
        L"带 x 前缀、步长为 0.1 的数值滑块。",
        L"可跨过零并显示负值的数值滑块。",
        L"以秒显示的数值滑块。", L"标签无需为数字的自定义滑块。"};
    static constexpr SliderText tchinese{
        L"進階滑桿展示", L"鏡頭距離", L"帶正負號偏移", L"路徑時間",
        L"不可能的值", L"展示帶正負號小數、單位、前綴與任意標籤。",
        L"帶 x 前綴、步進 0.1 的數值滑桿。",
        L"可跨過零並顯示負值的數值滑桿。",
        L"以秒數顯示的數值滑桿。", L"標籤不必是數字的自訂滑桿。"};
    static constexpr SliderText russian{
        L"Демонстрация расширенных ползунков", L"Расстояние камеры",
        L"Знаковое смещение", L"Время пути", L"Невозможные значения",
        L"Знаковые дроби, единицы, префиксы и произвольные подписи.",
        L"Числовой ползунок с префиксом x и шагом 0,1.",
        L"Числовой ползунок, проходящий через ноль и показывающий отрицательные значения.",
        L"Числовой ползунок со значением в секундах.",
        L"Пользунок с произвольными нечисловыми подписями."};
    static constexpr SliderText thai{
        L"ตัวอย่างแถบเลื่อนขั้นสูง", L"ระยะกล้อง", L"ออฟเซ็ตแบบมีเครื่องหมาย",
        L"เวลาเส้นทาง", L"ค่าที่เป็นไปไม่ได้",
        L"ทศนิยมมีเครื่องหมาย หน่วย คำนำหน้า และป้ายกำหนดเอง",
        L"แถบเลื่อนตัวเลขที่มีคำนำหน้า x และขั้นละ 0.1",
        L"แถบเลื่อนตัวเลขที่ข้ามศูนย์และแสดงค่าลบ",
        L"แถบเลื่อนตัวเลขที่แสดงเป็นวินาที",
        L"แถบเลื่อนกำหนดเองที่ป้ายไม่จำเป็นต้องเป็นตัวเลข"};
    static constexpr SliderText japanese{
        L"高度なスライダー・ショーケース", L"カメラ距離", L"符号付きオフセット",
        L"パス時間", L"ありえない値", L"符号付き小数、単位、接頭辞、自由なラベルを示します。",
        L"x 接頭辞と 0.1 刻みの数値スライダーです。",
        L"0 を跨いで負の値を表示する数値スライダーです。",
        L"秒単位で表示する数値スライダーです。",
        L"ラベルが数値でなくてもよいカスタムスライダーです。"};
    static constexpr SliderText polish{
        L"Prezentacja zaawansowanych suwaków", L"Odległość kamery",
        L"Przesunięcie ze znakiem", L"Czas trasy", L"Niemożliwe wartości",
        L"Ułamki ze znakiem, jednostki, prefiksy i dowolne etykiety wartości.",
        L"Suwak liczbowy z prefiksem x i krokiem 0,1.",
        L"Suwak liczbowy przechodzący przez zero i pokazujący wartości ujemne.",
        L"Suwak liczbowy wyświetlany w sekundach.",
        L"Suwak niestandardowy, którego etykiety nie muszą być liczbami."};
    static constexpr SliderText arabic{
        L"عرض المنزلقات المتقدمة", L"مسافة الكاميرا", L"إزاحة موقعة",
        L"زمن المسار", L"قيم مستحيلة", L"كسور موقعة ووحدات وبادئات وتسميات حرة.",
        L"منزلق رقمي بالبادئة x وخطوات 0.1.",
        L"منزلق رقمي يعبر الصفر ويعرض قيمًا سالبة.",
        L"منزلق رقمي معروض بالثواني.", L"منزلق مخصص لا يشترط أن تكون تسمياته أرقامًا."};
    static constexpr SliderText brazilian{
        L"Demonstração de controles deslizantes avançados", L"Distância da câmera",
        L"Deslocamento com sinal", L"Tempo do percurso", L"Valores impossíveis",
        L"Decimais com sinal, unidades, prefixos e rótulos arbitrários.",
        L"Um controle numérico com prefixo x e passos de 0,1.",
        L"Um controle numérico que cruza zero e mostra valores negativos.",
        L"Um controle numérico exibido em segundos.",
        L"Um controle personalizado cujos rótulos não precisam ser números."};
    static constexpr SliderText latam{
        L"Demostración de controles deslizantes avanzados", L"Distancia de cámara",
        L"Desplazamiento con signo", L"Tiempo de recorrido", L"Valores imposibles",
        L"Decimales con signo, unidades, prefijos y etiquetas arbitrarias.",
        L"Un control numérico con prefijo x y pasos de 0,1.",
        L"Un control numérico que cruza cero y muestra valores negativos.",
        L"Un control numérico mostrado en segundos.",
        L"Un control personalizado cuyas etiquetas no tienen que ser números."};
    static constexpr SliderText vietnamese{
        L"Trình diễn thanh trượt nâng cao", L"Khoảng cách camera",
        L"Độ lệch có dấu", L"Thời gian đường đi", L"Giá trị bất khả thi",
        L"Số thập phân có dấu, đơn vị, tiền tố và nhãn tùy ý.",
        L"Thanh trượt số hiển thị với tiền tố x và bước 0.1.",
        L"Thanh trượt số đi qua 0 và hiển thị giá trị âm.",
        L"Thanh trượt số hiển thị theo giây.",
        L"Thanh trượt tùy chỉnh có nhãn không cần là số."};

    if (is_vietnamese(language)) return vietnamese;
    switch (language.known) {
    case erui::GameLanguage::german: return german;
    case erui::GameLanguage::french: return french;
    case erui::GameLanguage::italian: return italian;
    case erui::GameLanguage::korean: return korean;
    case erui::GameLanguage::spanish: return spanish;
    case erui::GameLanguage::chinese_simplified: return schinese;
    case erui::GameLanguage::chinese_traditional: return tchinese;
    case erui::GameLanguage::russian: return russian;
    case erui::GameLanguage::thai: return thai;
    case erui::GameLanguage::japanese: return japanese;
    case erui::GameLanguage::polish: return polish;
    case erui::GameLanguage::arabic: return arabic;
    case erui::GameLanguage::portuguese_brazil: return brazilian;
    case erui::GameLanguage::spanish_latin_america: return latam;
    default: return english;
    }
}

} // namespace showcase
