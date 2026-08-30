#include <ernativeui/ERNativeUI.hpp>

#include <Windows.h>

namespace {

struct Text {
    const wchar_t* name;
    const wchar_t* button;
    const wchar_t* help;
    const wchar_t* greeting;
};

constexpr Text kEnglish{
    L"Localized Greeting", L"Show Greeting",
    L"Show a message in Elden Ring's selected language.",
    L"Hello, Tarnished!"};

constexpr Text localized(erui::GameLanguage language) noexcept {
    switch (language) {
    case erui::GameLanguage::german:
        return {L"Lokalisierte Begrüßung", L"Begrüßung anzeigen",
            L"Zeigt eine Nachricht in der ausgewählten Spielsprache an.",
            L"Seid gegrüßt, Befleckter!"};
    case erui::GameLanguage::french:
        return {L"Salutation localisée", L"Afficher la salutation",
            L"Affiche un message dans la langue sélectionnée du jeu.",
            L"Bonjour, Sans-éclat !"};
    case erui::GameLanguage::italian:
        return {L"Saluto localizzato", L"Mostra saluto",
            L"Mostra un messaggio nella lingua selezionata del gioco.",
            L"Salve, Senzaluce!"};
    case erui::GameLanguage::korean:
        return {L"현지화된 인사", L"인사 표시",
            L"선택한 게임 언어로 메시지를 표시합니다.", L"안녕하세요, 빛바랜 자여!"};
    case erui::GameLanguage::spanish:
        return {L"Saludo localizado", L"Mostrar saludo",
            L"Muestra un mensaje en el idioma seleccionado del juego.",
            L"¡Hola, Sinluz!"};
    case erui::GameLanguage::chinese_simplified:
        return {L"本地化问候", L"显示问候",
            L"使用游戏当前选择的语言显示消息。", L"你好，褪色者！"};
    case erui::GameLanguage::chinese_traditional:
        return {L"本地化問候", L"顯示問候",
            L"使用遊戲目前選擇的語言顯示訊息。", L"你好，褪色者！"};
    case erui::GameLanguage::russian:
        return {L"Локализованное приветствие", L"Показать приветствие",
            L"Показывает сообщение на выбранном языке игры.",
            L"Здравствуй, Погасший!"};
    case erui::GameLanguage::thai:
        return {L"คำทักทายที่แปลแล้ว", L"แสดงคำทักทาย",
            L"แสดงข้อความในภาษาที่เลือกของเกม", L"สวัสดี ผู้มัวหมอง!"};
    case erui::GameLanguage::japanese:
        return {L"ローカライズされた挨拶", L"挨拶を表示",
            L"ゲームで選択中の言語でメッセージを表示します。", L"こんにちは、褪せ人よ！"};
    case erui::GameLanguage::polish:
        return {L"Lokalne powitanie", L"Pokaż powitanie",
            L"Wyświetla wiadomość w wybranym języku gry.",
            L"Witaj, Zmatowieńcze!"};
    case erui::GameLanguage::arabic:
        return {L"تحية مترجمة", L"إظهار التحية",
            L"يعرض رسالة بلغة اللعبة المحددة.", L"مرحبًا أيها المشوّه!"};
    case erui::GameLanguage::portuguese_brazil:
        return {L"Saudação localizada", L"Mostrar saudação",
            L"Mostra uma mensagem no idioma selecionado do jogo.",
            L"Olá, Maculado!"};
    case erui::GameLanguage::spanish_latin_america:
        return {L"Saludo localizado", L"Mostrar saludo",
            L"Muestra un mensaje en el idioma seleccionado del juego.",
            L"¡Hola, Sinluz!"};
    default:
        return kEnglish;
    }
}

HMODULE g_module{};
erui::Registration g_registration{};
Text g_text{kEnglish};

void show_greeting() noexcept {
    (void)g_registration.alert(g_text.greeting);
}

DWORD WINAPI initialize(void*) noexcept {
    // Query before registration so even the provider name can be localized.
    // UNKNOWN and unavailable languages deliberately fall back to English.
    const erui::LanguageInfo language = erui::query_game_language();
    g_text = localized(language.known);

    erui::ProviderOptions options{};
    options.provider_id = "org.ernativeui.example.localized-greeting";
    options.display_name = g_text.name;
    options.owner_module = g_module;

    auto result = erui::register_menu(options, [](erui::Menu& menu) {
        menu.root().add_button<&show_greeting>(
            g_text.button, g_text.help);
    });
    if (result) g_registration = result.value();
    return result ? 0u : 1u;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, &initialize, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
